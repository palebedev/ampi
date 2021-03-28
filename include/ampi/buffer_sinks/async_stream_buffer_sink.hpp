// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_5E84D5A1_AA30_42B1_8AD3_D4A000BCA4EA
#define UUID_5E84D5A1_AA30_42B1_8AD3_D4A000BCA4EA

#include <ampi/buffer_sources/buffer_source.hpp>
#include <ampi/coro/use_coroutine.hpp>

#include <boost/asio/write.hpp>
#include <boost/container/static_vector.hpp>

namespace ampi
{
    using default_iovec_t = boost::container::static_vector<cbuffer,128>;

    template<executor Executor,typename AsyncWriteStream>
    coroutine<void,Executor> async_stream_buffer_sink(Executor ex,AsyncWriteStream& stream,
                                                      buffer_source auto& bs,auto& iovec)
    {
        iovec.clear();
        auto drain = [](Executor,AsyncWriteStream& stream,auto& iovec)
                -> async_subgenerator<std::nullptr_t,Executor> {
            for(;;){
                co_await boost::asio::async_write(stream,iovec,use_coroutine);
                iovec.clear();
                co_yield {};
            }
        }(ex,stream,iovec);
        while(auto buf = co_await bs)
            if(iovec.empty()||!iovec.back().append(*buf)){
                iovec.emplace_back(std::move(*buf));
                if(iovec.size()==iovec.capacity())
                    co_await drain;
            }
        co_await drain;
    }

    template<typename AsyncWriteStream>
    auto async_stream_buffer_sink(AsyncWriteStream& stream,buffer_source auto& bs,auto& iovec)
        -> coroutine<void,decltype(stream.get_executor())>
    {
        return async_stream_buffer_sink(stream.get_executor(),stream,bs,iovec);
    }
}

#endif
