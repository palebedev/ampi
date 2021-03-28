// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_A4C4C3D7_5899_46FE_B7E1_7393FA6B471C
#define UUID_A4C4C3D7_5899_46FE_B7E1_7393FA6B471C

#include <ampi/buffer_sources/istream_buffer_source.hpp>
#include <ampi/detail/msgunpack_ctx_base.hpp>
#include <ampi/event_sinks/event_sink.hpp>
#include <ampi/manipulator.hpp>

namespace ampi
{
    template<readahead_t ReadAhead>
    class istream_msgpack_ctx : public msgunpack_ctx_base_with_parser<
        generator<buffer,pmr_system_executor>>
    {
    public:
        explicit istream_msgpack_ctx(std::istream& stream,shared_polymorphic_allocator<> spa = {},
                                     parser_options po = {},segmented_stack_resource ssr = {}) noexcept
            : msgunpack_ctx_base_with_parser{[&,this](pmr_buffer_factory& bf){
                  return istream_buffer_source<ReadAhead>(ctx_.ex,stream,bf);
              },std::move(spa),po,std::move(ssr)}
        {}

        template<deserializable T>
            requires (!std::is_const_v<T>)
        istream_msgpack_ctx& operator>>(T& x)
        {
            auto p_v = this->p_();
            auto sink = serial_event_sink(type_tag<T>)(ctx_.ex,p_v,x).assume_blocking();
            sink();
            return *this;
        }
    };

    namespace detail
    {
        template<deserializable T>
            requires (!std::is_const_v<T>)
        std::istream& operator>>(std::istream& stream,as_msgpack_t<T> amt)
        {
            istream_msgpack_ctx<readahead_t::none>{stream} >> amt.x;
            return stream;
        }
    }
}

#endif
