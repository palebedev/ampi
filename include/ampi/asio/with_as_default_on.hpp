// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_F8186669_7558_4239_88C4_0C8E9DE80D46
#define UUID_F8186669_7558_4239_88C4_0C8E9DE80D46

#include <ampi/execution/executor_wrapper.hpp>

namespace ampi
{
    template<typename CompletionToken>
    struct with_as_default_on
    {
        template<executor Executor>
        class executor_with_default
            : public executor_wrapper<executor_with_default,Executor>
        {
            using base_t = executor_wrapper<executor_with_default,Executor>;
        public:
            using default_completion_token_type = CompletionToken;

            using base_t::base_t;
        };

        template<typename T>
        using as_default_on_t = typename T::template rebind_executor<
            executor_with_default<typename T::executor_type>>::other;

        template<typename IOObject>
        static as_default_on_t<IOObject> as_default_on(IOObject io_object)
        {
            return as_default_on_t<IOObject>{std::move(io_object)};
        }
    };
}

#endif
