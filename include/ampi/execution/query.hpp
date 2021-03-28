// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_1120BF46_5829_47A2_AD55_AB5DA064A8C4
#define UUID_1120BF46_5829_47A2_AD55_AB5DA064A8C4

#include <boost/asio/query.hpp>

namespace ampi
{
    template<typename T,typename Property>
    using query_result_t = typename boost::asio::query_result<T,Property>::type;
}

#endif
