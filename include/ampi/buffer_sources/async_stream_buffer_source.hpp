// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_41E57842_C9B6_4BFC_AB6E_9BE046FD20B1
#define UUID_41E57842_C9B6_4BFC_AB6E_9BE046FD20B1

#include <ampi/asio/as_tuple.hpp>
#include <ampi/buffer_sources/buffer_source.hpp>
#include <ampi/coro/use_coroutine.hpp>

#include <boost/asio/read.hpp>
#include <boost/system/system_error.hpp>

namespace ampi
{
    template<typename T>
    concept readahead_available_stream = requires(T stream,buffer buf){
        stream.async_wait(T::wait_read,static_cast<void (*)(boost::system::error_code)>(nullptr));
        { stream.available() } -> std::convertible_to<size_t>;
    };

    template<readahead_t ReadAhead,executor Executor,typename AsyncReadStream>
        requires (ReadAhead==readahead_t::none)||
                 readahead_available_stream<AsyncReadStream>
    async_generator<cbuffer,Executor> async_stream_buffer_source(Executor /*ex*/,
            AsyncReadStream& stream,buffer_factory auto& bf)
    {
        for(;;){
            buffer buf;
            if constexpr(ReadAhead==readahead_t::available){
                co_await stream.async_wait(AsyncReadStream::wait_read,use_coroutine);
                size_t n = stream.available();
                if(!n)
                    break;
                buf = bf.get_buffer(n);
                if(buf.size()==n){
                    [[maybe_unused]] size_t n2 = stream.read_some(boost::asio::mutable_buffer(buf));
                    assert(n==n2);
                    co_yield std::move(buf);
                    continue;
                }
            }else
                buf = bf.get_buffer();
            auto [ec,n] = co_await boost::asio::async_read(
                stream,boost::asio::mutable_buffer(buf),as_tuple(use_coroutine));
            if(ec&&ec!=boost::asio::error::eof)
                throw boost::system::system_error(ec);
            if(n)
                co_yield buffer(std::move(buf),0,n);
            if(ec==boost::asio::error::eof)
                break;
        }
    }

    template<readahead_t ReadAhead,typename AsyncReadStream>
        requires (ReadAhead==readahead_t::none)||
                 readahead_available_stream<AsyncReadStream>
    auto async_stream_buffer_source(AsyncReadStream& stream,buffer_factory auto& bf)
        -> async_generator<cbuffer,decltype(stream.get_executor())>
    {
        return async_stream_buffer_source<ReadAhead>(stream.get_executor(),stream,bf);
    }
}

#endif
