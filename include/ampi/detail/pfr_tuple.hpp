// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_59342635_257C_44FD_B18B_1B9A4420D32F
#define UUID_59342635_257C_44FD_B18B_1B9A4420D32F

#include <ampi/event_endpoints.hpp>

#include <boost/hana/concept/struct.hpp>
#include <boost/pfr/core.hpp>
#include <boost/pfr/tuple_size.hpp>

namespace ampi::detail
{
    template<typename T>
    concept pfr_tuple = std::is_aggregate_v<T>&&
                        !std::is_array_v<T>&&
                        !container_like<T>&&
                        !boost::hana::Struct<T>::value;
}

#endif
