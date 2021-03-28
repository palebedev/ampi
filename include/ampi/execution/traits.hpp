// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_655C6AD6_E576_4778_868A_E118A6631E1D
#define UUID_655C6AD6_E576_4778_868A_E118A6631E1D

#include <boost/asio/system_executor.hpp>

#include <type_traits>

namespace ampi
{
    template<executor Executor>
    struct is_trivial_executor : std::false_type {};

    template<typename Blocking,typename Relationship,typename Allocator>
    struct is_trivial_executor<
            boost::asio::basic_system_executor<Blocking,Relationship,Allocator>>
        : std::bool_constant<!std::is_same_v<Blocking,boost::asio::execution::blocking_t::never_t>> {};

    template<executor Executor>
    constexpr inline bool is_trivial_executor_v = is_trivial_executor<Executor>{};
}

#endif
