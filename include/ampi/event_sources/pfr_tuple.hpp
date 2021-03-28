// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_19B40254_9008_410C_8A33_95B207448929
#define UUID_19B40254_9008_410C_8A33_95B207448929

#include <ampi/detail/pfr_tuple.hpp>
#include <ampi/event_sources/event_source.hpp>

namespace ampi::serial_event_source_ns
{
    namespace detail
    {
        template<ampi::detail::pfr_tuple T>
        constexpr bool serializable_pfr_tuple() noexcept
        {
            constexpr auto n = boost::pfr::tuple_size_v<T>;
            if(n>0xffffffff)
                return false;
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return (...&&serializable<boost::pfr::tuple_element_t<Indices,T>>);
            }(std::make_index_sequence<n>{});
        }
    }

    template<typename T>
    concept serializable_pfr_tuple = detail::serializable_pfr_tuple<T>();

    template<serializable_pfr_tuple T>
    auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
    {
        return []<size_t... Indices>(std::index_sequence<Indices...>){
            return [](pmr_system_executor ex,const T& x) -> delegating_event_generator {
                co_yield sequence_header{uint32_t(boost::pfr::tuple_size_v<T>)};
                (...,(co_yield serial_event_source(type_tag<boost::pfr::tuple_element_t<Indices,T>>)
                    (ex,boost::pfr::get<Indices>(x))));
            };
        }(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
    }
}

#endif
