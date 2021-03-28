// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_CACA5936_A497_40B1_B28A_8B853A4A7009
#define UUID_CACA5936_A497_40B1_B28A_8B853A4A7009

#include <boost/asio/require.hpp>

namespace ampi
{
    template<typename T,typename... Properties>
    using require_result_t = typename boost::asio::require_result<T,Properties...>::type;
}

#endif
