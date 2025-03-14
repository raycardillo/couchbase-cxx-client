/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *   Copyright 2020-2021 Couchbase, Inc.
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

#include "analytics_dataverse_create.hxx"

#include "core/utils/json.hxx"
#include "core/utils/name_codec.hxx"
#include "error_utils.hxx"

namespace couchbase::core::operations::management
{
std::error_code
analytics_dataverse_create_request::encode_to(encoded_request_type& encoded, http_context& /* context */) const
{
    std::string if_not_exists_clause = ignore_if_exists ? "IF NOT EXISTS" : "";

    tao::json::value body{
        { "statement", fmt::format("CREATE DATAVERSE {} {}", utils::analytics::uncompound_name(dataverse_name), if_not_exists_clause) },
    };
    encoded.headers["content-type"] = "application/json";
    encoded.method = "POST";
    encoded.path = "/analytics/service";
    encoded.body = utils::json::generate(body);
    return {};
}

analytics_dataverse_create_response
analytics_dataverse_create_request::make_response(error_context::http&& ctx, const encoded_response_type& encoded) const
{
    analytics_dataverse_create_response response{ std::move(ctx) };
    if (!response.ctx.ec) {
        tao::json::value payload{};
        try {
            payload = utils::json::parse(encoded.body.data());
        } catch (const tao::pegtl::parse_error&) {
            response.ctx.ec = errc::common::parsing_failure;
            return response;
        }
        response.status = payload.optional<std::string>("status").value_or("unknown");

        if (response.status != "success") {
            bool dataverse_exists = false;

            if (auto* errors = payload.find("errors"); errors != nullptr && errors->is_array()) {
                for (const auto& error : errors->get_array()) {
                    analytics_problem err{
                        error.at("code").as<std::uint32_t>(),
                        error.at("msg").get_string(),
                    };
                    switch (err.code) {
                        case 24039: /* A dataverse with this name [string] already exists. */
                            dataverse_exists = true;
                            break;
                    }
                    response.errors.emplace_back(err);
                }
            }
            if (dataverse_exists) {
                response.ctx.ec = errc::analytics::dataverse_exists;
            } else {
                response.ctx.ec = extract_common_error_code(encoded.status_code, encoded.body.data());
            }
        }
    }
    return response;
}
} // namespace couchbase::core::operations::management
