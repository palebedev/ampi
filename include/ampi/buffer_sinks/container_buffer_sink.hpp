// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_A6E62876_6DA3_454D_AD82_3834F990995D
#define UUID_A6E62876_6DA3_454D_AD82_3834F990995D

#include <ampi/buffer_sources/buffer_source.hpp>

namespace ampi
{
    template<executor Executor>
    coroutine<void,Executor> container_buffer_sink(Executor /*ex*/,auto& cont,buffer_source auto& bs)
    {
        while(auto buf = co_await bs)
            cont.insert(cont.end(),buf->begin(),buf->end());
    }

    coroutine<> container_buffer_sink(auto& cont,buffer_source auto& bs)
    {
        return container_buffer_sink(boost::asio::system_executor{},cont,bs);
    }
}

#endif
