// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_B358DADD_D819_4355_8964_98D5EA91EB3C
#define UUID_B358DADD_D819_4355_8964_98D5EA91EB3C

#include <ampi/buffer_sources/buffer_source.hpp>
#include <ampi/event.hpp>

#include <boost/endian/conversion.hpp>

namespace ampi
{
    namespace detail
    {
        template<typename T>
        static void put_big_endian(byte* p,T value) noexcept
        {
            boost::endian::endian_store<T,sizeof(T),boost::endian::order::big>(
                reinterpret_cast<unsigned char*>(p),value);
        }
    }

    template<executor Executor>
    async_generator<cbuffer,Executor> emitter(Executor /*ex*/,event_source auto& es,
                                              buffer_factory auto& bf)
    {
        auto write_byte = [&](byte b){
            buffer buf = bf.get_buffer(1);
            buf[0] = b;
            return buf;
        };
        auto write_big_endian = [&](byte prefix,auto x){
            buffer buf = bf.get_buffer(1+sizeof(decltype(x)));
            buf[0] = prefix;
            detail::put_big_endian(buf.data()+1,x);
            return buf;
        };
        auto write_124 = [&](uint8_t prefix_base,uint32_t size){
            buffer buf;
            if(size<=0xff){
                buf = bf.get_buffer(1+1);
                buf[0] = byte{prefix_base};
                buf[1] = byte{uint8_t(size)};
            }else if(size<=0xffff){
                buf = bf.get_buffer(1+2);
                buf[0] = byte{uint8_t(prefix_base+1)};
                detail::put_big_endian(buf.data()+1,uint16_t(size));
            }else{
                buf = bf.get_buffer(1+4);
                buf[0] = byte{uint8_t(prefix_base+2)};
                detail::put_big_endian(buf.data()+1,uint32_t(size));
            }
            return buf;
        };
        while(event* e = co_await es){
            uint64_t x;
            switch(e->kind()){
                case object_kind::null:
                    co_yield write_byte(byte{0xc0});
                    break;
                case object_kind::bool_:
                    co_yield write_byte(byte{uint8_t(0xc2+*e->get_if<bool>())});
                    break;
                case object_kind::signed_int:
                    if(int64_t sx = *e->get_if<int64_t>();sx<0){
                        if(sx>=-0x20)
                            co_yield write_byte(byte{uint8_t(sx)});
                        else if(sx>=-0x80)
                            co_yield write_big_endian(byte{0xd0},uint8_t(sx));
                        else if(sx>=-int32_t(0x8000))
                            co_yield write_big_endian(byte{0xd1},uint16_t(sx));
                        else if(sx>=-int64_t(0x80000000))
                            co_yield write_big_endian(byte{0xd2},uint32_t(sx));
                        else
                            co_yield write_big_endian(byte{0xd3},uint64_t(sx));
                        break;
                    }else{
                        x = uint64_t(sx);
                        goto have_positive;
                    }
                case object_kind::unsigned_int:
                    {
                        x = *e->get_if<uint64_t>();
have_positive:
                        if(x<=0x7f)
                            co_yield write_byte(byte{uint8_t(x)});
                        else if(x<=0xff)
                            co_yield write_big_endian(byte{0xcc},uint8_t(x));
                        else if(x<=0xffff)
                            co_yield write_big_endian(byte{0xcd},uint16_t(x));
                        else if(x<=0xffffffff)
                            co_yield write_big_endian(byte{0xce},uint32_t(x));
                        else
                            co_yield write_big_endian(byte{0xcf},uint64_t(x));
                    }
                    break;
                case object_kind::float_:
                    co_yield write_big_endian(byte{0xca},*e->get_if<float>());
                    break;
                case object_kind::double_:
                    co_yield write_big_endian(byte{0xcb},*e->get_if<double>());
                    break;
                case object_kind::sequence:
                case object_kind::map:
                    {
                        bool is_map = e->kind()==object_kind::map;
                        uint32_t n = is_map?
                            e->get_if<map_header>()->size:
                            e->get_if<sequence_header>()->size;
                        if(n<=0xf)
                            co_yield write_byte(byte{uint8_t((0x90^uint8_t(is_map<<4))|n)});
                        else if(n<=0xffff)
                            co_yield write_big_endian(byte{uint8_t(0xdc|(is_map<<1))},
                                                        uint16_t(n));
                        else
                            co_yield write_big_endian(byte{uint8_t(0xdd|(is_map<<1))},n);
                    }
                    break;
                case object_kind::string:
                    {
                        uint32_t size = e->get_if<string_header>()->size;
                        if(size<=0x1f)
                            co_yield write_byte(byte{uint8_t(0xa0+size)});
                        else
                            co_yield write_124(0xd9,size);
                    }
                    break;
                case object_kind::binary:
                    co_yield write_124(0xc4,e->get_if<binary_header>()->size);
                    break;
                case object_kind::extension:
                    {
                        auto& ext = *e->get_if<extension_header>();
                        buffer buf;
                        if(std::has_single_bit(ext.size)&&ext.size<=16){
                            buf = bf.get_buffer(1+1);
                            buf[0] = byte{uint8_t(0xd3+std::bit_width(ext.size))};
                        }else if(ext.size<=0xff){
                            buf = bf.get_buffer(1+1+1);
                            buf[0] = byte{0xc7};
                            buf[1] = byte{uint8_t(ext.size)};
                        }else if(ext.size<=0xffff){
                            buf = bf.get_buffer(1+2+1);
                            buf[0] = byte{0xc8};
                            detail::put_big_endian(buf.data()+1,uint16_t(ext.size));
                        }else{
                            buf = bf.get_buffer(1+4+1);
                            buf[0] = byte{0xc9};
                            detail::put_big_endian(buf.data()+1,uint32_t(ext.size));
                        }
                        buf[buf.size()-1] = byte{uint8_t(ext.type)};
                        co_yield std::move(buf);
                    }
                    break;
                case object_kind::timestamp:
                    {
                        int64_t ns = e->get_if<timestamp_t>()->time_since_epoch().count(),
                                s = ns/1'000'000'000;
                        ns %= 1'000'000'000;
                        if(ns<0){
                            --s;
                            ns += 1'000'000'000;
                        }
                        buffer buf;
                        if(s>>34){
                            buf= bf.get_buffer(1+1+1+4+8);
                            buf[0] = byte{0xc7};
                            buf[1] = byte{12};
                            buf[2] = byte{uint8_t(-1)};
                            detail::put_big_endian(buf.data()+1+1+1,uint32_t(ns));
                            detail::put_big_endian(buf.data()+1+1+1+4,s);
                        }else{
                            uint64_t d = (uint64_t(ns)<<34)|uint64_t(s);
                            if(d>>32){
                                buf = bf.get_buffer(1+1+8);
                                buf[0] = byte{0xd7};
                                buf[1] = byte{uint8_t(-1)};
                                detail::put_big_endian(buf.data()+1+1,d);
                            }else{
                                buf = bf.get_buffer(1+1+4);
                                buf[0] = byte{0xd6};
                                buf[1] = byte{uint8_t(-1)};
                                detail::put_big_endian(buf.data()+1+1,uint32_t(d));
                            }
                        }
                        co_yield std::move(buf);
                    }
                    break;
                default: // case object_kind::data_buffer:
                    co_yield std::move(*e->get_if<cbuffer>());
            }
        }
    }

    async_generator<cbuffer> emitter(event_source auto& es,buffer_factory auto& bf)
    {
        return emitter(boost::asio::system_executor{},es,bf);
    }
}

#endif
