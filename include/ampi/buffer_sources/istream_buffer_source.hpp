// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_BB7698D8_1AF9_4061_B605_C41C6F472CB1
#define UUID_BB7698D8_1AF9_4061_B605_C41C6F472CB1

#include <ampi/buffer_sources/buffer_source.hpp>

#include <istream>

namespace ampi
{
    template<readahead_t ReadAhead,executor Executor>
    generator<buffer,Executor> istream_buffer_source(Executor /*ex*/,std::istream& stream,
                                                     buffer_factory auto& bf)
    {
        while(stream){
            buffer buf = bf.get_buffer(size_t(
                ReadAhead==readahead_t::available?
                    std::max<std::streamsize>(0,stream.rdbuf()->in_avail()):0));
            if(!stream.read(reinterpret_cast<char*>(buf.data()),std::streamsize(buf.size()))||
                    !stream.gcount())
                co_return;
            co_yield {std::move(buf),0,size_t(stream.gcount())};
        }
    }

    template<readahead_t ReadAhead>
    generator<buffer> istream_buffer_source(std::istream& stream,buffer_factory auto& bf)
    {
        return istream_buffer_source<ReadAhead>(boost::asio::system_executor{},stream,bf);
    }
}

#endif
