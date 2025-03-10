/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2022-Present Couchbase, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
 * ANY KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#pragma once

#include "core_sdk_shim.hxx"
#include "key_value_config.hxx"
#include "seed_config.hxx"

#include <memory>
#include <string>

namespace couchbase
{
class retry_strategy;
} // namespace couchbase

namespace couchbase::core
{
struct cluster_agent_config {
    core_sdk_shim shim{}; // TODO: remove once refactoring will be finished.

    std::string user_agent{};
    std::shared_ptr<retry_strategy> default_retry_strategy{};

    seed_config seed{};
    key_value_config key_value{};

    [[nodiscard]] auto to_string() const -> std::string;
};
} // namespace couchbase::core
