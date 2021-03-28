// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_E46124CA_714F_4210_AF0A_AADA3A903E88
#define UUID_E46124CA_714F_4210_AF0A_AADA3A903E88

#include <ampi/coro/traits.hpp>

namespace ampi
{
    template<awaiter Awaiter>
    struct awaiter_wrapper
    {
        Awaiter a_;

        bool await_ready()
            noexcept(noexcept(a_.await_ready()))
        {
            return a_.await_ready();
        }

        template<typename Promise>
        auto await_suspend(stdcoro::coroutine_handle<Promise> handle)
            noexcept(noexcept(a_.await_suspend(handle)))
        {
            return a_.await_suspend(handle);
        }

        decltype(auto) await_resume()
            noexcept(noexcept(a_.await_resume()))
        {
            return a_.await_resume();
        }
    };
}

#endif
