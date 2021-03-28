// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_DB3AA6D4_A3A2_492A_9C1F_E730BBD24187
#define UUID_DB3AA6D4_A3A2_492A_9C1F_E730BBD24187

#include <ampi/buffer_sources/buffer_source.hpp>

#include <ostream>

namespace ampi
{
    template<typename Executor>
    coroutine<void,Executor> ostream_buffer_sink(Executor /*ex*/,std::ostream& stream,
                                                 buffer_source auto& bs)
    {
        while(auto buf = co_await bs)
            if(!stream.write(reinterpret_cast<const char*>(buf->data()),
                             std::streamsize(buf->size())))
                break;
    }

    coroutine<> ostream_buffer_sink(std::ostream& stream,buffer_source auto& bs)
    {
        return ostream_buffer_sink(boost::asio::system_executor{},stream,bs);
    }
}

#endif
