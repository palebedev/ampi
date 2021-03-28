// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_993B3294_A113_406A_83E2_7AB28151A3AE
#define UUID_993B3294_A113_406A_83E2_7AB28151A3AE

#include <ampi/event_sinks/event_sink.hpp>
#include <ampi/event_sources/event_source.hpp>

namespace ampi
{
    template<deserializable T,serializable U>
    void transmute(T& x,const U& y)
    {
        [](pmr_system_executor ex1,pmr_system_executor ex2,T& x,const U& y)
                -> subcoroutine<void,pmr_system_executor> {
            auto source = serial_event_source(type_tag<U>)(ex1,y);
            auto sink = serial_event_sink(type_tag<T>)(ex2,source,x);
            co_await std::move(sink);
            if(auto e = co_await source)
                throw structure_error{structure_error::reason_t::unexpected_event,
                                      boost::typeindex::type_id<T>(),{},std::move(*e)};
        }(detail::stack_executor_ctx{}.ex,detail::stack_executor_ctx{}.ex,x,y)();
    }
    
    template<deserializable T,serializable U>
    T transmute(const U& x)
    {
        T y;
        transmute(y,x);
        return y;
    }
}

#endif
