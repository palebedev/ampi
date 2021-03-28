// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_13F2D7C4_24A6_4B8C_AF92_A2BA749D05E3
#define UUID_13F2D7C4_24A6_4B8C_AF92_A2BA749D05E3

#include <ampi/detail/hana_struct.hpp>
#include <ampi/event_sources/event_source.hpp>

namespace ampi::serial_event_source_ns
{
    namespace detail
    {
        template<typename T>
            requires boost::hana::Struct<T>::value
        constexpr bool serializable_hana_struct() noexcept
        {
            auto a = boost::hana::accessors<T>();
            constexpr size_t n = boost::hana::size(a);
            if(n>0xffffffff)
                return false;
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return (...&&serializable<ampi::detail::accessor_result_t<
                    decltype(boost::hana::second(a[boost::hana::size_c<Indices>])
                    (std::declval<T>()))>>);
            }(std::make_index_sequence<n>{});
        }
    }

    template<typename T>
    concept serializable_hana_struct = detail::serializable_hana_struct<T>();

    template<serializable_hana_struct T>
    auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
    {
        return []<size_t... Indices>(std::index_sequence<Indices...>){
            return [](pmr_system_executor ex,const T& x) -> delegating_event_generator {
                auto a = boost::hana::accessors<T>();
                co_yield map_header{check_size<T>(boost::hana::size(a).value)};
                auto str_ses = serial_event_source(type_tag<std::string_view>);
                auto get_ses = [&](auto i) -> decltype(auto) {
                    decltype(auto) v = boost::hana::second(a[i])(x);
                    return serial_event_source(type_tag<ampi::detail::accessor_result_t<decltype(v)>>)(ex,v);
                };
                (...,(co_yield str_ses(ex,boost::hana::first(
                          a[boost::hana::size_c<Indices>]).c_str()),
                      co_yield get_ses(boost::hana::size_c<Indices>)));
            };
        }(std::make_index_sequence<boost::hana::size(boost::hana::accessors<T>())>{});
    }
}

#endif
