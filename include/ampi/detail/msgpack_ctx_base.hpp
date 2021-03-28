// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_4B62FD23_A6D1_4FD7_825E_2CDB1C023A67
#define UUID_4B62FD23_A6D1_4FD7_825E_2CDB1C023A67

#include <ampi/event_endpoints.hpp>

namespace ampi
{
    class msgpack_ctx_base
    {
    public:
        explicit msgpack_ctx_base(segmented_stack_resource ssr) noexcept
            : ctx_{{std::move(ssr)}}
        {} 
    protected:
        detail::stack_executor_ctx ctx_;
    };
}
#endif
