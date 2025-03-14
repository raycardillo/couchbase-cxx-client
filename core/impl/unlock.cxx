/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *   Copyright 2020-Present Couchbase, Inc.
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

#include "core/cluster.hxx"
#include "core/error_context/key_value.hxx"
#include "core/operations/document_unlock.hxx"

#include <couchbase/unlock_options.hxx>

namespace couchbase::core::impl
{
void
initiate_unlock_operation(std::shared_ptr<couchbase::core::cluster> core,
                          std::string bucket_name,
                          std::string scope_name,
                          std::string collection_name,
                          std::string document_key,
                          couchbase::cas cas,
                          unlock_options::built options,
                          unlock_handler&& handler)
{
    core->execute(
      operations::unlock_request{
        document_id{ std::move(bucket_name), std::move(scope_name), std::move(collection_name), std::move(document_key) },
        {},
        {},
        cas,
        options.timeout,
        { options.retry_strategy },
      },
      [handler = std::move(handler)](operations::unlock_response&& resp) mutable { return handler(std::move(resp.ctx)); });
}
} // namespace couchbase::core::impl
