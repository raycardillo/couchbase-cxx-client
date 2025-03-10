/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *   Copyright 2023-Present Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "pillowfight.hxx"

#include "core/operations/document_upsert.hxx"
#include "couchbase/codec/binary_noop_serializer.hxx"
#include "utils.hxx"

#include <core/logger/logger.hxx>
#include <couchbase/cluster.hxx>
#include <couchbase/codec/raw_binary_transcoder.hxx>
#include <couchbase/fmt/cas.hxx>

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <fmt/chrono.h>

#include <csignal>
#include <numeric>
#include <random>

namespace cbc
{
namespace
{
auto
usage() -> std::string
{
    static const std::string default_bucket_name{ "default" };
    static const std::size_t default_number_of_io_threads{ 1 };
    static const std::size_t default_number_of_worker_threads{ 1 };
    static const double default_chance_of_get{ 0.6 };
    static const double default_hit_chance_for_get{ 1.0 };
    static const double default_hit_chance_for_upsert{ 0.5 };
    static const double default_chance_of_query{ 0.0 };
    static const std::string default_query_statement{ "SELECT COUNT(*) FROM `{bucket_name}` WHERE type = \"fake_profile\"" };
    static const std::size_t default_document_body_size{ 0 };
    static const std::size_t default_operation_limit{ 0 };

    static const std::string usage_string = fmt::format(
      R"(Run workload generator.

Usage:
  cbc pillowfight [options]
  cbc pillowfight (-h|--help)

Options:
  -h --help                           Show this screen.
  --bucket-name=STRING                Name of the bucket. [default: {bucket_name}]
  --scope-name=STRING                 Name of the scope. [default: {scope_name}]
  --collection-name=STRING            Name of the collection. [default: {collection_name}]
  --number-of-io-threads=INTEGER      Number of the IO threads. [default: {number_of_io_threads}]
  --number-of-worker-threads=INTEGER  Number of the IO threads. [default: {number_of_worker_threads}]
  --chance-of-get=FLOAT               The probability of get operation (where 1 means only get, and 0 - only upsert). [default: {chance_of_get}]
  --hit-chance-for-get=FLOAT          The probability of using existing ID for get operation. [default: {hit_chance_for_get}]
  --hit-chance-for-upsert=FLOAT       The probability of using existing ID for upsert operation. [default: {hit_chance_for_upsert}]
  --chance-of-query=FLOAT             The probability of N1QL query will be send on after get/upsert. [default: {chance_of_query}]
  --query-statement=STRING            The N1QL query statement to use ({{bucket_name}}, {{scope_name}} and {{collection_name}} will be substituted). [default: {query_statement}]
  --incompressible-body               Use random characters to fill generated document value (by default uses 'x' to fill the body).
  --document-body-size=INTEGER        Size of the body (if zero, it will use predefined document). [default: {document_body_size}]
  --operations-limit=INTEGER          Stop and exit after the number of the operations reaches this limit. (zero for running indefinitely) [default: {operation_limit}]

{logger_options}{cluster_options}
)",
      fmt::arg("bucket_name", default_bucket_name),
      fmt::arg("scope_name", couchbase::scope::default_name),
      fmt::arg("collection_name", couchbase::collection::default_name),
      fmt::arg("number_of_io_threads", default_number_of_io_threads),
      fmt::arg("number_of_worker_threads", default_number_of_worker_threads),
      fmt::arg("chance_of_get", default_chance_of_get),
      fmt::arg("hit_chance_for_get", default_hit_chance_for_get),
      fmt::arg("hit_chance_for_upsert", default_hit_chance_for_upsert),
      fmt::arg("chance_of_query", default_chance_of_query),
      fmt::arg("query_statement", default_query_statement),
      fmt::arg("document_body_size", default_document_body_size),
      fmt::arg("operation_limit", default_operation_limit),
      fmt::arg("logger_options", usage_block_for_logger()),
      fmt::arg("cluster_options", usage_block_for_cluster_options()));

    return usage_string;
}

const char* default_json_doc = R"({
  "type": "fake_profile",
  "random": 91,
  "random float": 16.439,
  "bool": false,
  "date": "1996-10-23",
  "regEx": "hellooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo to you",
  "enum": "online",
  "firstname": "Anthia",
  "lastname": "Shields",
  "city": "Recife",
  "country": "Malawi",
  "countryCode": "CA",
  "email uses current data": "Anthia.Shields@gmail.com",
  "email from expression": "Anthia.Shields@yopmail.com",
  "array": [
    "Augustine",
    "Pearline",
    "Fredericka",
    "Dorothy",
    "Roz"
  ],
  "array of objects": [
    {
      "index": 0,
      "index start at 5": 5
    },
    {
      "index": 1,
      "index start at 5": 6
    },
    {
      "index": 2,
      "index start at 5": 7
    }
  ],
  "Mildrid": {
    "age": 33
  }
})";

struct command_options {
    std::string bucket_name;
    std::string scope_name;
    std::string collection_name;
    std::size_t number_of_io_threads;
    std::size_t number_of_worker_threads;
    double chance_of_get;
    double hit_chance_for_get;
    double hit_chance_for_upsert;
    double chance_of_query;
    std::string query_statement;
    bool incompressible_body;
    std::size_t document_body_size;
};

enum class operation {
    get,
    upsert,
};

using raw_json_transcoder = couchbase::codec::json_transcoder<couchbase::codec::binary_noop_serializer>;

std::atomic_flag running{ true };
std::size_t operations_limit{ 0 };
std::atomic_uint64_t total{ 0 };

std::map<std::error_code, std::size_t> errors{};
std::mutex errors_mutex{};

void
sigint_handler(int /* signal */)
{
    running.clear();
}

void
dump_stats(asio::steady_timer& timer, std::chrono::system_clock::time_point start_time)
{
    timer.expires_after(std::chrono::seconds{ 1 });
    timer.async_wait([&timer, start_time](std::error_code ec) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_time).count();
        const std::uint64_t ops = total;
        fmt::print(stderr, "\rrate: {} ops/s, total: {}", diff == 0 ? ops : ops / static_cast<std::uint64_t>(diff), ops);
        return dump_stats(timer, start_time);
    });
}

std::string
uniq_id(const std::string& prefix)
{
    return fmt::format("{}_{}", prefix, std::chrono::steady_clock::now().time_since_epoch().count());
}

std::string
random_text(std::size_t length)
{
    std::string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937_64 gen{ std::random_device()() };
    std::uniform_int_distribution<std::size_t> dis(0, alphabet.size() - 1);
    std::string text(length, '-');
    for (std::size_t i = 0; i < length; ++i) {
        text[i] = alphabet[dis(gen)];
    }
    return text;
}

void
worker(couchbase::cluster connected_cluster, command_options cmd_options, std::vector<std::string>& known_keys)
{
    auto options = std::move(cmd_options);
    auto cluster = std::move(connected_cluster);

    static thread_local std::mt19937_64 gen{ std::random_device()() };
    std::uniform_real_distribution<double> dist(0, 1);

    auto collection = cluster.bucket(options.bucket_name).scope(options.scope_name).collection(options.collection_name);

    std::vector<std::byte> json_doc{};
    if (options.document_body_size > 0) {
        json_doc = couchbase::core::utils::json::generate_binary({
          { "size", options.document_body_size },
          { "text", options.incompressible_body ? random_text(options.document_body_size) : std::string(options.document_body_size, 'x') },
        });
    } else {
        json_doc = couchbase::core::utils::to_binary(default_json_doc);
    }

    while (running.test_and_set()) {
        auto opcode = (dist(gen) <= options.chance_of_get) ? operation::get : operation::upsert;
        if (opcode == operation::get && known_keys.empty()) {
            opcode = operation::upsert;
        }
        bool should_check_known_keys{ false };
        switch (opcode) {
            case operation::get:
                should_check_known_keys = options.hit_chance_for_get > dist(gen);
                break;
            case operation::upsert:
                should_check_known_keys = options.hit_chance_for_upsert > dist(gen);
                break;
        }
        std::string document_id = uniq_id("id");
        if (should_check_known_keys && !known_keys.empty()) {
            auto key_index = std::uniform_int_distribution<std::size_t>(0, known_keys.size() - 1)(gen);
            document_id = known_keys[key_index];
        } else {
            known_keys.emplace_back(document_id);
        }

        switch (opcode) {
            case operation::upsert: {
                const couchbase::upsert_options operation_options{};
                auto [ctx, resp] = collection.upsert<raw_json_transcoder>(document_id, json_doc, operation_options).get();
                ++total;
                if (ctx.ec()) {
                    const std::scoped_lock lock(errors_mutex);
                    ++errors[ctx.ec()];
                }
            } break;
            case operation::get: {
                const couchbase::get_options operation_options{};
                auto [ctx, resp] = collection.get(document_id, operation_options).get();
                ++total;
                if (ctx.ec()) {
                    const std::scoped_lock lock(errors_mutex);
                    ++errors[ctx.ec()];
                }
            } break;
        }
        if (options.chance_of_query > 0 && dist(gen) <= options.chance_of_query) {
            const couchbase::query_options operation_options{};
            auto [ctx, resp] = cluster.query(options.query_statement, operation_options).get();
            ++total;
            if (ctx.ec()) {
                const std::scoped_lock lock(errors_mutex);
                ++errors[ctx.ec()];
            }
        }
        if (operations_limit > 0 && total >= operations_limit) {
            running.clear();
        }
    }
}

void
do_work(const std::string& connection_string, const couchbase::cluster_options& cluster_options, const command_options& cmd_options)
{
    asio::io_context io;
    auto guard = asio::make_work_guard(io);

    std::vector<std::thread> io_pool{};
    io_pool.reserve(cmd_options.number_of_io_threads);
    for (std::size_t i = 0; i < cmd_options.number_of_io_threads; ++i) {
        io_pool.emplace_back([&io]() { io.run(); });
    }

    std::signal(SIGINT, sigint_handler);
    std::signal(SIGTERM, sigint_handler);

    auto [cluster, ec] = couchbase::cluster::connect(io, connection_string, cluster_options).get();
    if (ec) {
        guard.reset();

        for (auto& thread : io_pool) {
            thread.join();
        }

        throw std::system_error(ec, "unable to connect to the cluster in time");
    }

    const auto start_time = std::chrono::system_clock::now();

    asio::steady_timer stats_timer(io);
    dump_stats(stats_timer, start_time);

    std::vector<std::vector<std::string>> known_keys(cmd_options.number_of_worker_threads);

    std::vector<std::thread> worker_pool{};
    worker_pool.reserve(cmd_options.number_of_worker_threads);
    for (std::size_t i = 0; i < cmd_options.number_of_worker_threads; ++i) {
        worker_pool.emplace_back([cluster = cluster, &cmd_options, &keys = known_keys[i]]() { worker(cluster, cmd_options, keys); });
    }
    for (auto& thread : worker_pool) {
        thread.join();
    }

    const auto finish_time = std::chrono::system_clock::now();
    stats_timer.cancel();

    fmt::print("\n\ntotal operations: {}\n", total);
    fmt::print("total keys used: {}\n",
               std::accumulate(known_keys.begin(), known_keys.end(), 0, [](auto count, const auto& keys) { return count + keys.size(); }));
    const auto total_time = finish_time - start_time;
    fmt::print("total time: {}s ({}ms)\n",
               std::chrono::duration_cast<std::chrono::seconds>(total_time).count(),
               std::chrono::duration_cast<std::chrono::milliseconds>(total_time).count());
    if (auto diff = std::chrono::duration_cast<std::chrono::seconds>(total_time).count(); diff > 0) {
        fmt::print("total rate: {} ops/s\n", total / static_cast<std::uint64_t>(diff));
    }
    {
        std::scoped_lock lock(errors_mutex);
        if (!errors.empty()) {
            fmt::print("error stats:\n");
            for (auto [e, count] : errors) {
                fmt::print("    {}: {}\n", ec.message(), count);
            }
        }
    }

    cluster.close();
    guard.reset();

    for (auto& thread : io_pool) {
        thread.join();
    }
}
} // namespace

void
cbc::pillowfight::execute(const std::vector<std::string>& argv)
{
    auto options = cbc::parse_options(usage(), argv);
    if (options["--help"].asBool()) {
        fmt::print(stdout, "{}", usage());
        return;
    }

    cbc::apply_logger_options(options);

    couchbase::cluster_options cluster_options{ cbc::default_cluster_options() };
    std::string connection_string{ cbc::default_connection_string() };
    cbc::fill_cluster_options(options, cluster_options, connection_string);

    command_options cmd_options{};
    cmd_options.bucket_name = options["--bucket-name"].asString();
    cmd_options.scope_name = options["--scope-name"].asString();
    cmd_options.collection_name = options["--collection-name"].asString();
    cmd_options.number_of_io_threads = options["--number-of-io-threads"].asLong();
    cmd_options.number_of_worker_threads = options["--number-of-worker-threads"].asLong();
    cmd_options.chance_of_get = get_double_option(options, "--chance-of-get");
    cmd_options.hit_chance_for_get = get_double_option(options, "--hit-chance-for-get");
    cmd_options.hit_chance_for_upsert = get_double_option(options, "--hit-chance-for-upsert");
    cmd_options.chance_of_query = get_double_option(options, "--chance-of-query");
    auto query_statement_template = options["--query-statement"].asString();
    cmd_options.query_statement = fmt::format(query_statement_template,
                                              fmt::arg("bucket_name", cmd_options.bucket_name),
                                              fmt::arg("scope_name", cmd_options.scope_name),
                                              fmt::arg("collection_name", cmd_options.collection_name));
    cmd_options.incompressible_body = get_bool_option(options, "--incompressible-body");
    cmd_options.document_body_size = options["--document-body-size"].asLong();
    operations_limit = options["--operations-limit"].asLong();

    do_work(connection_string, cluster_options, cmd_options);
}
} // namespace cbc
