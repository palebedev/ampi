// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_79B01C14_D4A3_4BB8_B224_4156D8C0AA21
#define UUID_79B01C14_D4A3_4BB8_B224_4156D8C0AA21

#include <boost/container/vector.hpp>
#include <boost/container_hash/hash_fwd.hpp>

template<typename T,typename Allocator,typename Options>
struct boost::hash<boost::container::vector<T,Allocator,Options>>
{
    std::size_t operator()(const boost::container::vector<T,Allocator,Options>& v) const noexcept
    {
        return boost::hash_range(v.begin(),v.end());
    }
};

template<typename T,typename Allocator,typename Options>
struct std::hash<boost::container::vector<T,Allocator,Options>>
    : boost::hash<boost::container::vector<T,Allocator,Options>> {};

#endif
