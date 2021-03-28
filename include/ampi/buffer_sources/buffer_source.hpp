// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_1E2A8E01_B3A0_4D47_9C02_8C499A59AC33
#define UUID_1E2A8E01_B3A0_4D47_9C02_8C499A59AC33

#include <ampi/buffer.hpp>
#include <ampi/coro/coroutine.hpp>

namespace ampi
{
    template<typename T>
    concept buffer_source = async_generator_yielding<T,cbuffer>;

    enum struct readahead_t
    {
        none,
        available
    };
}

#endif
