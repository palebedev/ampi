// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_FA31A812_2CF7_46D2_A890_656A546439BF
#define UUID_FA31A812_2CF7_46D2_A890_656A546439BF

#include <ampi/event.hpp>

namespace ampi::detail
{
    class fixed_msgpack_buffer_factory : public null_buffer_factory
    {
    public:
        buffer get_buffer(size_t n = 0) noexcept
        {
            assert(n<=sizeof buf_);
            return binary_view_t{buf_,n};
        }
    private:
        byte buf_[max_msgpack_fixed_buffer_length];
    };
}

#endif
