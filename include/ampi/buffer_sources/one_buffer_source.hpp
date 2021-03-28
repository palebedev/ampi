// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_FEA29053_69C9_4CA5_8D32_4B5BE2C3587A
#define UUID_FEA29053_69C9_4CA5_8D32_4B5BE2C3587A

#include <ampi/buffer_sources/buffer_source.hpp>

namespace ampi
{
    template<executor Executor>
    noexcept_generator<cbuffer,Executor> one_buffer_source(Executor /*ex*/,binary_cview_t view)
    {
        co_yield {view};
    }

    inline noexcept_generator<cbuffer> one_buffer_source(binary_cview_t view)
    {
        return one_buffer_source(boost::asio::system_executor{},view);
    }
}

#endif
