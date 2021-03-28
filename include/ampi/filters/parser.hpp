// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_A264A743_2603_46A0_9462_1B2D5A9AAA54
#define UUID_A264A743_2603_46A0_9462_1B2D5A9AAA54

#include <ampi/buffer_sources/buffer_source.hpp>
#include <ampi/coro/coroutine.hpp>
#include <ampi/event.hpp>
#include <ampi/exception.hpp>
#include <ampi/utils/flags.hpp>
#include <ampi/utf8_validator.hpp>

#include <boost/endian/conversion.hpp>

namespace ampi
{
    class AMPI_EXPORT parser_error : public exception
    {
    public:
        enum struct reason_t
        {
            invalid_leading_byte,
            unexpected_end,
            invalid_utf8,
            invalid_timestamp_length,
            invalid_timestamp_nanoseconds,
            unsupported_timestamp
        };

        parser_error(reason_t reason) noexcept
            : reason_{reason}
        {}

        const char* what() const noexcept override;
    private:
        reason_t reason_;
    };

    enum struct parser_option : uint8_t
    {
        skip_utf8_validation = 0b1
    };

    using parser_options = flags<parser_option>;

    template<buffer_source BufferSource,buffer_factory BufferFactory,
             executor Executor = boost::asio::system_executor>
    class parser
    {
    public:
        parser(BufferSource& bs,BufferFactory& bf,parser_options options = {},Executor ex = {}) noexcept
            : bs_{bs},
              bf_{bf},
              options_{options},
              ex_{ex}
        {}

        async_generator<event,Executor> operator()() [[clang::lifetimebound]]
        {
            return [](Executor,parser& p) -> async_generator<event,Executor> {
                uint64_t remaining_items = 1;
                auto do_sequence = [&](uint32_t n){
                    remaining_items += n;
                    return sequence_header{n};
                };
                auto do_map = [&](uint32_t n){
                    remaining_items += n*2ull;
                    return map_header{n};
                };
                auto do_timestamp = [](int64_t s,uint32_t ns){
                    if(ns>999'999'999)
                        throw parser_error{parser_error::reason_t::invalid_timestamp_nanoseconds};
                    constexpr int64_t max_s = std::numeric_limits<int64_t>::max()/1'000'000'000,
                                      min_s = std::numeric_limits<int64_t>::lowest()/1'000'000'000-1;
                    constexpr uint32_t max_ns =
                            uint32_t(std::numeric_limits<int64_t>::max()%1'000'000'000),
                                       min_ns = uint32_t(1'000'000'000-
                                     std::numeric_limits<int64_t>::lowest()%1'000'000'000);
                    if(s>max_s||(s==max_s&&ns>max_ns)||s<min_s||(s==min_s&&ns<min_ns))
                        throw parser_error{parser_error::reason_t::unsupported_timestamp};
                    return timestamp_t{std::chrono::seconds{s}+
                                       std::chrono::nanoseconds{ns}};
                };
                uint32_t data_length = 0;
                bool string_data = false;
                do{
                    uint8_t first_byte = co_await p.get<uint8_t>();
                    if(first_byte<0x80)
                        co_yield uint64_t(first_byte);
                    else if(first_byte>=0xe0)
                        co_yield int64_t(int8_t(first_byte));
                    else if(first_byte<0x90)
                        co_yield do_map(first_byte&0xf);
                    else if(first_byte<0xa0)
                        co_yield do_sequence(first_byte&0xf);
                    else if(first_byte==0xc0)
                        co_yield nullptr;
                    else if((first_byte|1)==0xc3)
                        co_yield bool(first_byte&1);
                    else if(first_byte==0xc1) [[unlikely]]
                        throw parser_error{parser_error::reason_t::invalid_leading_byte};
                    else if(first_byte<0xc0){
                        data_length = first_byte&0x1f;
                        co_yield string_header{data_length};
                        string_data = true;
                    }else if(first_byte<0xc7){
                        data_length = co_await p.get_length(first_byte-0xc4);
                        co_yield binary_header{data_length};
                    }else if(first_byte<0xca){
                        data_length = co_await p.get_length(first_byte-0xc7);
                        int8_t type = co_await p.get<int8_t>();
                        if(type==-1){
                            if(data_length!=12)
                                throw parser_error{parser_error::reason_t::invalid_timestamp_length};
                            uint32_t ns = co_await p.get<uint32_t>();
                            int64_t s = co_await p.get<int64_t>();
                            co_yield do_timestamp(s,ns);
                        }else
                            co_yield extension_header{data_length,type};
                    }else if(first_byte==0xca){
                        // FIXME: must destroy get frame before returning to work
                        //        with segmented stack allocator.
                        float x = co_await p.get<float>();
                        co_yield x;
                    }else if(first_byte==0xcb){
                        double x = co_await p.get<double>();
                        co_yield x;
                    }else if(first_byte==0xcc){
                        uint8_t x = co_await p.get<uint8_t>();
                        co_yield uint64_t(x);
                    }else if(first_byte==0xcd){
                        uint16_t x = co_await p.get<uint16_t>();
                        co_yield uint64_t(x);
                    }else if(first_byte==0xce){
                        uint32_t x = co_await p.get<uint32_t>();
                        co_yield uint64_t(x);
                    }else if(first_byte==0xcf){
                        uint64_t x = co_await p.get<uint64_t>();
                        co_yield x;
                    }else if(first_byte==0xd0){
                        int8_t x = co_await p.get<int8_t>();
                        co_yield int64_t(x);
                    }else if(first_byte==0xd1){
                        int16_t x = co_await p.get<int16_t>();
                        co_yield int64_t(x);
                    }else if(first_byte==0xd2){
                        int32_t x = co_await p.get<int32_t>();
                        co_yield int64_t(x);
                    }else if(first_byte==0xd3){
                        int64_t x = co_await p.get<int64_t>();
                        co_yield x;
                    }else if(first_byte<0xd9){
                        data_length = 1u<<(first_byte-0xd4);
                        int8_t type = co_await p.get<int8_t>();
                        if(type==-1)
                            switch(data_length){
                                case 4:
                                    {
                                        uint32_t t = co_await p.get<uint32_t>();
                                        co_yield timestamp_t{std::chrono::seconds(t)};
                                    }
                                    break;
                                case 8:
                                    {
                                        uint64_t t = co_await p.get<uint64_t>();
                                        co_yield do_timestamp(t&0x3ffffffff,t>>34);
                                    }
                                    break;
                                default:
                                    throw parser_error{parser_error::reason_t::invalid_timestamp_length};
                            }
                        else
                            co_yield extension_header{data_length,type};
                    }else if(first_byte<0xdc){
                        data_length = co_await p.get_length(first_byte-0xd9);
                        co_yield string_header{data_length};
                        string_data = true;
                    }else if(first_byte<0xde){
                        uint32_t n = co_await p.get_length(first_byte-0xdb);
                        co_yield do_sequence(n);
                    }else{
                        uint32_t n = co_await p.get_length(first_byte-0xdd);
                        co_yield do_map(n);
                    }
                    if(data_length){
                        bool check_utf8 = string_data&&
                                          !(p.options_&parser_option::skip_utf8_validation);
                        utf8_validator u8v;
                        cbuffer* b; 
                        size_t c;
                        if(p.buf_){
                            b = &p.buf_;
                            goto have_buffer;
                        }
                        do{
                            p.bf_.next_buffer_size(data_length);
                            b = co_await p.bs_;
                            if(!b)
                                throw parser_error{parser_error::reason_t::unexpected_end};
have_buffer:
                            c = std::min<size_t>(data_length,b->size());
                            cbuffer ret{*b,0,c};
                            if(check_utf8&&
                                    u8v({reinterpret_cast<const char*>(ret.data()),ret.size()}))
                                throw parser_error{parser_error::reason_t::invalid_utf8};
                            data_length -= uint32_t(c);
                            if(!data_length){
                                if(check_utf8&&!u8v)
                                    throw parser_error{parser_error::reason_t::invalid_utf8};
                                if(c<b->size())
                                    p.buf_ = {std::move(*b),c};
                                else
                                    p.buf_ = {};
                            }
                            co_yield std::move(ret);
                        }while(data_length);
                    }
                    string_data = false;
                }while(--remaining_items);
            }(ex_,*this);
        }

        Executor get_executor() const noexcept
        {
            return ex_;
        }

        void set_initial(cbuffer buf) noexcept
        {
            buf_ = std::move(buf);
        }

        cbuffer rest() noexcept
        {
            return std::move(buf_);
        }
    private:
        BufferSource& bs_;
        BufferFactory& bf_;
        parser_options options_;
        Executor ex_;
        cbuffer buf_;

        template<typename T>
        auto get() [[clang::lifetimebound]]
        {
            return [](Executor,parser& p) -> subcoroutine<T,Executor> {
                constexpr size_t n = sizeof(T);
                const void* d;
                char merge_buf[max_msgpack_fixed_buffer_length];
                if(p.buf_.size()>=sizeof(T)){
                    d = p.buf_.data();
                    p.buf_ = {p.buf_,n};
                }else{
                    size_t c,i = p.buf_.size();
                    cbuffer* b;
                    if(i)
                        std::memcpy(merge_buf,p.buf_.data(),i);
                    do{
                        size_t left = n-i;
                        p.bf_.next_buffer_size(left);
                        b = co_await p.bs_;
                        if(!b)
                            throw parser_error{parser_error::reason_t::unexpected_end};
                        c = std::min(left,b->size());
                        std::memcpy(merge_buf+i,b->data(),c);
                        i += c;
                    }while(i<n);
                    if(c<b->size())
                        p.buf_ = {std::move(*b),c};
                    else
                        p.buf_ = {};
                    d = merge_buf;
                }
                co_return boost::endian::endian_load<T,n,boost::endian::order::big>(
                    static_cast<const unsigned char*>(d));
            }(ex_,*this);
        }

        subcoroutine<uint32_t,Executor> get_length(uint8_t power) [[clang::lifetimebound]]
        {
            return [](Executor,parser& p,uint8_t power) -> subcoroutine<uint32_t,Executor> {
                if(!power)
                    co_return co_await p.get<uint8_t>();
                if(power==1)
                    co_return co_await p.get<uint16_t>();
                co_return co_await p.get<uint32_t>();
            }(ex_,*this,power);
        }
    };
}

#endif
