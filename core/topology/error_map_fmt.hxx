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

#pragma once

#include "error_map.hxx"

#include <fmt/core.h>

template<>
struct fmt::formatter<couchbase::api::key_value_error_map_attribute> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(couchbase::api::key_value_error_map_attribute attr, FormatContext& ctx) const
    {
        string_view name = "unknown";
        switch (attr) {
            case couchbase::api::key_value_error_map_attribute::success:
                name = "success";
                break;
            case couchbase::api::key_value_error_map_attribute::item_only:
                name = "item-only";
                break;
            case couchbase::api::key_value_error_map_attribute::invalid_input:
                name = "invalid-input";
                break;
            case couchbase::api::key_value_error_map_attribute::fetch_config:
                name = "fetch-config";
                break;
            case couchbase::api::key_value_error_map_attribute::conn_state_invalidated:
                name = "conn-state-invalidated";
                break;
            case couchbase::api::key_value_error_map_attribute::auth:
                name = "auth";
                break;
            case couchbase::api::key_value_error_map_attribute::special_handling:
                name = "special-handling";
                break;
            case couchbase::api::key_value_error_map_attribute::support:
                name = "support";
                break;
            case couchbase::api::key_value_error_map_attribute::temp:
                name = "temp";
                break;
            case couchbase::api::key_value_error_map_attribute::internal:
                name = "internal";
                break;
            case couchbase::api::key_value_error_map_attribute::retry_now:
                name = "retry-now";
                break;
            case couchbase::api::key_value_error_map_attribute::retry_later:
                name = "retry-later";
                break;
            case couchbase::api::key_value_error_map_attribute::subdoc:
                name = "subdoc";
                break;
            case couchbase::api::key_value_error_map_attribute::dcp:
                name = "dcp";
                break;
            case couchbase::api::key_value_error_map_attribute::auto_retry:
                name = "auto-retry";
                break;
            case couchbase::api::key_value_error_map_attribute::item_locked:
                name = "item-locked";
                break;
            case couchbase::api::key_value_error_map_attribute::item_deleted:
                name = "item-deleted";
                break;
            case couchbase::api::key_value_error_map_attribute::rate_limit:
                name = "rate-limit";
                break;
        }
        return format_to(ctx.out(), "{}", name);
    }
};
