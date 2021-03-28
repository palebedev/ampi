// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_E8B5A410_4A58_44DC_A61D_DECFB7448433
#define UUID_E8B5A410_4A58_44DC_A61D_DECFB7448433

#include <ampi/event_endpoints.hpp>

namespace ampi
{
    using event_generator = generator<event,pmr_system_executor>;
    using noexcept_event_generator = noexcept_generator<event,pmr_system_executor>;
    using delegating_event_generator = delegating_generator<event,pmr_system_executor>;

    namespace serial_event_source_ns
    {
        namespace detail
        {
            template<typename T,typename U>
            concept serial_event_source_factory = requires(T sesf,pmr_system_executor ex,const U& x){
                { sesf(ex,x) } -> generator_yielding_exactly<event>;
            };
        }

        constexpr inline struct serial_event_source_fn
        {
            template<typename T>
                requires detail::serial_event_source_factory<
                    tag_invoke_result_t<serial_event_source_fn,type_tag_t<T>>,
                    T
                >
            auto operator()(type_tag_t<T> tt) const noexcept
            {
                return ampi::tag_invoke(*this,tt);
            }
        } serial_event_source;
    }

    using serial_event_source_ns::serial_event_source;

    template<typename T>
    concept serializable = is_tag_invocable_v<tag_t<serial_event_source>,type_tag_t<T>>;

    namespace serial_event_source_ns
    {
        namespace detail
        {
            template<tuple_like T>
                requires (!container_like<T>)
            constexpr bool serializable_tuple() noexcept
            {
                constexpr auto n = std::tuple_size_v<T>;
                if(n>0xffffffff)
                    return false;
                return []<size_t... Indices>(std::index_sequence<Indices...>){
                    return (...&&serializable<std::tuple_element_t<Indices,T>>);
                }(std::make_index_sequence<n>{});
            }
        }

        template<typename T>
        concept serializable_tuple_like = detail::serializable_tuple<T>();

        template<typename T>
        uint32_t check_size(integral auto n)
        {
            if(std::cmp_greater(n,0xffffffff))
                throw structure_error{structure_error::reason_t::out_of_range,
                                    boost::typeindex::type_id<T>()};
            return uint32_t(n);
        }

        template<typename T>
            requires std::is_same_v<T,std::nullptr_t>||
                    std::is_same_v<T,bool>||
                    std::is_same_v<T,float>||
                    std::is_same_v<T,double>||
                    std::is_same_v<T,timestamp_t>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor,T x) -> noexcept_event_generator {
                co_yield x;
            };
        }

        template<integral T>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor,T x) -> noexcept_event_generator {
                if(x<0)
                    co_yield int64_t(x);
                else
                    co_yield uint64_t(x);
            };
        }

        template<std::convertible_to<string_view> T>
            requires (!std::is_same_v<T,std::nullptr_t>)
        inline auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor,string_view x) -> noexcept_event_generator {
                co_yield string_header{check_size<T>(x.size())};
                co_yield cbuffer{{reinterpret_cast<const byte*>(x.data()),x.size()}};
            };
        }

        template<std::convertible_to<binary_cview_t> T>
        inline auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor,binary_cview_t x) -> noexcept_event_generator {
                co_yield binary_header{check_size<T>(x.size())};
                co_yield cbuffer{x};
            };
        }
        
        template<serializable T>
            requires regular_optional_state<T>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<optional<T>>) noexcept
        {
            return [](pmr_system_executor ex,const optional<T>& x) -> delegating_event_generator {
                if(x)
                    co_yield tail_yield_to(serial_event_source(type_tag<T>)(ex,*x));
                else
                    co_yield nullptr;
            };
        }

        template<serializable... Ts>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<variant<Ts...>>) noexcept
        {
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return [](pmr_system_executor ex,const variant<Ts...>& x) -> delegating_event_generator {
                    co_yield sequence_header{2};
                    auto i = x.index();
                    co_yield i;
                    static_cast<void>((...||(i==Indices&&(co_yield tail_yield_to(
                        serial_event_source(type_tag<Ts>)(ex,*get_if<Indices>(&x))),true))));
                };
            }(std::index_sequence_for<Ts...>{});
        }

        template<serializable_tuple_like T>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return [](pmr_system_executor ex,const T& x) -> delegating_event_generator {
                    co_yield sequence_header{uint32_t(std::tuple_size_v<T>)};
                    (...,(co_yield serial_event_source(type_tag<std::tuple_element_t<Indices,T>>)
                        (ex,get<Indices>(x))));
                };
            }(std::make_index_sequence<std::tuple_size_v<T>>{});
        }

        template<container_like T>
            requires (!std::convertible_to<T,string_view>)&&
                     (!std::convertible_to<T,binary_cview_t>)&&
                     (!map_like<T>)&&
                     serializable<container_value_type_t<T>>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor ex,const T& x) -> delegating_event_generator {
                co_yield sequence_header{check_size<T>(std::size(x))};
                auto ses = serial_event_source(type_tag<container_value_type_t<T>>);
                for(auto& e:x)
                    co_yield ses(ex,e);
            };
        }

        template<map_like T>
            requires serializable<std::remove_const_t<map_like_key_type<T>>>&&
                    serializable<map_like_mapped_type<T>>
        auto tag_invoke(tag_t<serial_event_source>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor ex,const T& x) -> delegating_event_generator {
                co_yield map_header{check_size<T>(std::size(x))};
                auto sesk = serial_event_source(type_tag<std::remove_const_t<map_like_key_type<T>>>);
                auto sesv = serial_event_source(type_tag<map_like_mapped_type<T>>);
                for(auto& [k,v]:x){
                    co_yield sesk(ex,k);
                    co_yield sesv(ex,v);
                }
            };
        }
    }
}

#endif
