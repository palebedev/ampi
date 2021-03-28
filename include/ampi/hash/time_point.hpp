// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_243592E6_7E51_446A_BAC3_91630AF5C76B
#define UUID_243592E6_7E51_446A_BAC3_91630AF5C76B

#include <boost/container_hash/hash.hpp>

#include <chrono>

template<typename Clock,typename Duration>
struct boost::hash<std::chrono::time_point<Clock,Duration>>
{
    std::size_t operator()(std::chrono::time_point<Clock,Duration> tp) const noexcept
    {
        auto c = tp.time_since_epoch().count();
        return boost::hash<decltype(c)>{}(c);
    }
};

template<typename Clock,typename Duration>
struct std::hash<std::chrono::time_point<Clock,Duration>>
    : boost::hash<std::chrono::time_point<Clock,Duration>> {};

#endif
