/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *   Copyright 2022-Present Couchbase, Inc.
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

#include "../utils/movable_function.hxx"

#include <functional>
#include <memory>
#include <system_error>

namespace couchbase::core::mcbp
{
class queue_request;
class queue_response;

using queue_callback =
  utils::movable_function<void(std::shared_ptr<queue_response> response, std::shared_ptr<queue_request> request, std::error_code error)>;

} // namespace couchbase::core::mcbp
