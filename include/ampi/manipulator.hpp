// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_1FA310A0_3940_4323_AB52_714E92E7208B
#define UUID_1FA310A0_3940_4323_AB52_714E92E7208B

namespace ampi
{
    namespace detail
    {
        template<typename T>
        struct as_msgpack_t
        {
            T& x;
        };
    }

    template<typename T>
    auto as_msgpack(T& x)
    {
        return detail::as_msgpack_t<T>{x};
    }

    template<typename T>
    auto as_msgpack(const auto& x)
    {
        return detail::as_msgpack_t<const T>{x};
    }
}

#endif
