// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_903043EF_DFE0_45A3_BF59_8E6B36BB409B
#define UUID_903043EF_DFE0_45A3_BF59_8E6B36BB409B

#include <ampi/detail/pfr_tuple.hpp>
#include <ampi/event_sinks/event_sink.hpp>

namespace ampi::serial_event_sink_ns
{
    namespace detail
    {
        template<ampi::detail::pfr_tuple T>
        constexpr bool deserializable_pfr_tuple() noexcept
        {
            constexpr auto n = boost::pfr::tuple_size_v<T>;
            if(n>0xffffffff)
                return false;
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return (...&&deserializable<boost::pfr::tuple_element_t<Indices,T>>);
            }(std::make_index_sequence<n>{});
        }
    }

    template<typename T>
    concept deserializable_pfr_tuple = detail::deserializable_pfr_tuple<T>();

    template<deserializable_pfr_tuple T>
    auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<T>) noexcept
    {
        constexpr auto n = boost::pfr::tuple_size_v<T>;
        return []<size_t... Indices>(std::index_sequence<Indices...>){
            return [](pmr_system_executor ex,event_source auto& source,T& x) -> async_event_consumer {
                event e = expect_event<T>(co_await source,{object_kind::sequence});
                if(e.get_if<sequence_header>()->size!=n)
                    throw structure_error{structure_error::reason_t::out_of_range,
                                          boost::typeindex::type_id<T>(),
                                          {object_kind::sequence},std::move(e)};
                (...,(co_await serial_event_sink(type_tag<boost::pfr::tuple_element_t<Indices,T>>)
                    (ex,source,boost::pfr::get<Indices>(x))));
            };
        }(std::make_index_sequence<n>{});
    }
}

#endif
