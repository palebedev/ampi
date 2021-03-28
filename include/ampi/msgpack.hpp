// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_6012704F_D6C6_4442_AC01_724C0376DC68
#define UUID_6012704F_D6C6_4442_AC01_724C0376DC68

#include <ampi/buffer_sinks/container_buffer_sink.hpp>
#include <ampi/buffer_sources/one_buffer_source.hpp>
#include <ampi/detail/fixed_msgpack_buffer_factory.hpp>
#include <ampi/detail/msgunpack_ctx_base.hpp>
#include <ampi/event_sinks/event_sink.hpp>
#include <ampi/event_sources/event_source.hpp>
#include <ampi/filters/emitter.hpp>
#include <ampi/filters/parser.hpp>

namespace ampi
{
    class value;

    class msgpack_ctx : public msgunpack_ctx_base
    {
    public:
        using msgunpack_ctx_base::msgunpack_ctx_base;

        template<serializable T>
        void msgpack(auto& cont,const T& x)
        {
            auto ses = serial_event_source(type_tag<T>)(ctx_.ex,x);
            detail::fixed_msgpack_buffer_factory bf;
            auto em = emitter(ctx_.ex,ses,bf);
            auto sink = container_buffer_sink(ctx_.ex,cont,em).assume_blocking();
            sink();
        }

        template<typename Container = vector<byte>>
        Container msgpack(const serializable auto& x)
        {
            Container cont;
            msgpack(cont,x);
            return cont;
        }

        template<deserializable T>
        void msgunpack(T& x,binary_cview_t view)
        {
            auto obs = one_buffer_source(ctx_.ex,view);
            null_buffer_factory bf;
            parser p{obs,bf,po_,ctx_.ex};
            auto p_v = p();
            auto sink = serial_event_sink(type_tag<T>)(ctx_.ex,p_v,x).assume_blocking();
            sink();
        }

        template<typename T = value>
        T msgunpack(binary_cview_t view)
        {
            T x;
            msgunpack(x,view);
            return x;
        }
    };

    template<serializable T>
    void msgpack(auto& cont,const T& x)
    {
        msgpack_ctx{}.msgpack(cont,x);
    }

    template<typename Container = vector<byte>>
    Container msgpack(const serializable auto& x)
    {
        return msgpack_ctx{}.msgpack<Container>(x);
    }

    template<deserializable T>
    void msgunpack(T& x,binary_cview_t view)
    {
        msgpack_ctx{}.msgunpack(x,view);
    }

    template<typename T = value>
    T msgunpack(binary_cview_t view)
    {
        return msgpack_ctx{}.msgunpack<T>(view);
    }
}

#endif
