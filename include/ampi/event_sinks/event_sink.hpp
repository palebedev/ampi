// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_AE8C6F44_6A99_451C_B58F_487698EA0A60
#define UUID_AE8C6F44_6A99_451C_B58F_487698EA0A60

#include <ampi/event_endpoints.hpp>

#include <limits>

namespace ampi
{
    using async_event_consumer = coroutine<void,pmr_system_executor>;

    namespace serial_event_sink_ns
    {
        namespace detail
        {
            template<typename T,typename U>
            concept serial_event_sink_factory = requires(T sesf,pmr_system_executor ex,
                                                        async_generator<event>& es,U& x) {
                { sesf(ex,es,x) } -> coroutine_returning<void>;
            };
        }

        constexpr inline struct serial_event_sink_fn
        {
            template<typename T>
                requires detail::serial_event_sink_factory<
                    tag_invoke_result_t<serial_event_sink_fn,type_tag_t<T>>,
                    T
                >
            auto operator()(type_tag_t<T> tt) const noexcept
            {
                return ampi::tag_invoke(*this,tt);
            }
        } serial_event_sink;
    }

    using serial_event_sink_ns::serial_event_sink;

    template<typename T>
    concept deserializable = is_tag_invocable_v<tag_t<serial_event_sink>,type_tag_t<T>>;

    namespace serial_event_sink_ns
    {
        namespace detail
        {
            template<tuple_like T>
                requires (!container_like<T>)
            constexpr bool deserializable_tuple() noexcept
            {
                constexpr auto n = std::tuple_size_v<T>;
                if(n>0xffffffff)
                    return false;
                return []<size_t... Indices>(std::index_sequence<Indices...>){
                    return (...&&deserializable<std::tuple_element_t<Indices,T>>);
                }(std::make_index_sequence<n>{});
            }
        }

        template<typename T>
        concept deserializable_tuple_like = detail::deserializable_tuple<T>();

        template<typename T>
        concept overwritable_container_like = container_like<T> &&
            requires(T& x,container_value_type_t<T> v) {
                *std::begin(x) = v;
            };

        template<typename T>
        concept resizeable_container_like = overwritable_container_like<T> && requires(T x) {
            x.resize(std::size(x));
        };

        template<typename T>
        concept emplace_back_container_like = container_like<T> && requires(T x) {
            x.clear();
            { x.emplace_back() } -> std::convertible_to<typename T::value_type&>;
        };

        template<typename T>
        concept set_container_like = container_like<T> && requires(T x) {
            x.clear();
            x.emplace(std::declval<typename T::value_type>());
        };

        template<typename T>
        concept emplaceable_map_like = map_like<T> && requires(T m,
                typename std::tuple_element_t<0,T> k,typename std::tuple_element_t<1,T> v) {
            m.emplace(std::move(k),std::move(v));
        };

        template<typename T>
        concept unique_associative_container_like = container_like<T> && requires(T m,
                typename T::value_type v) {
            { get<1>(m.emplace(std::move(v))) } -> std::convertible_to<bool>;
        };

        template<typename T,typename Res>
        void check_unique_key(Res res)
        {
            if constexpr(unique_associative_container_like<T>)
                if(!get<1>(res))
                    throw structure_error{structure_error::reason_t::duplicate_key,
                                        boost::typeindex::type_id<T>()};
        }

        template<typename T>
        event expect_event(event* e,object_kind_set expected = object_kind_set::any)
        {
            if(!e)
                throw structure_error{structure_error::reason_t::unexpected_event,
                                    boost::typeindex::type_id<T>(),expected};
            if(!(e->kind()&expected))
                throw structure_error{structure_error::reason_t::unexpected_event,
                                    boost::typeindex::type_id<T>(),expected,std::move(*e)};
            return std::move(*e);
        }

        async_subgenerator<event,pmr_system_executor> first_event_repeater
                (pmr_system_executor,event_source auto &es)
        {
            if(auto e = co_await es){
                co_yield event{*e};
                co_yield std::move(*e);
                while((e = co_await es))
                    co_yield std::move(*e);
            }
        }

        inline auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<std::nullptr_t>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,std::nullptr_t& /*x*/)
                    -> async_event_consumer {
                expect_event<std::nullptr_t>(co_await source,{object_kind::null});
            };
        }

        inline auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<bool>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,bool& x) -> async_event_consumer {
                x = *expect_event<bool>(co_await source,{object_kind::bool_}).template get_if<bool>();
            };
        }

        template<integral T>
        auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,T& x) -> async_event_consumer {
                event e = expect_event<T>(co_await source,
                                        {object_kind::unsigned_int,object_kind::signed_int});
                auto throw_out_of_range = [&]{
                    throw structure_error{structure_error::reason_t::out_of_range,
                                        boost::typeindex::type_id<T>(),{},std::move(e)};
                };
                if(e.kind()==object_kind::unsigned_int){
                    uint64_t v = *e.get_if<uint64_t>();
                    if(std::cmp_greater(v,std::numeric_limits<T>::max()))
                        throw_out_of_range();
                    x = T(v);
                }else{
                    int64_t v = *e.get_if<int64_t>();
                    if(std::cmp_greater(v,std::numeric_limits<T>::max())||
                            std::cmp_less(v,std::numeric_limits<T>::lowest()))
                        throw_out_of_range();
                    x = T(v);
                }
            };
        }

        inline auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<float>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,float& x) -> async_event_consumer {
                x = *expect_event<float>(co_await source,{object_kind::float_}).template get_if<float>();
            };
        }

        inline auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<double>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,double& x) -> async_event_consumer {
                x = *expect_event<double>(co_await source,{object_kind::double_}).template get_if<double>();
            };
        }

        inline auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<timestamp_t>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,timestamp_t& x) -> async_event_consumer {
                x = *expect_event<timestamp_t>(co_await source,{object_kind::timestamp}).
                        template get_if<timestamp_t>();
            };
        }

        template<typename Allocator>
        inline auto tag_invoke(tag_t<serial_event_sink>,
                            type_tag_t<std::basic_string<char,std::char_traits<char>,Allocator>>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,auto& x) -> async_event_consumer {
                uint32_t n = expect_event<decltype(x)>(co_await source,{object_kind::string})
                                .template get_if<string_header>()->size;
                x.reserve(n);
                while(n){
                    cbuffer buf = std::move(*expect_event<decltype(x)>(co_await source,
                        {object_kind::data_buffer}).template get_if<cbuffer>());
                    auto p = reinterpret_cast<const char*>(buf.data());
                    x.insert(x.end(),p,p+buf.size());
                    n -= uint32_t(buf.size());
                }
            };
        }

        template<typename Allocator>
        inline auto tag_invoke(tag_t<serial_event_sink>,
                            type_tag_t<boost::container::vector<byte,Allocator>>) noexcept
        {
            return [](pmr_system_executor,event_source auto& source,auto& x) -> async_event_consumer {
                uint32_t n = expect_event<decltype(x)>(co_await source,{object_kind::binary})
                                .template get_if<binary_header>()->size;
                x.reserve(n);
                while(n){
                    cbuffer buf = std::move(*expect_event<decltype(x)>(co_await source,
                        {object_kind::data_buffer}).template get_if<cbuffer>());
                    x.insert(x.end(),buf.data(),buf.data()+buf.size());
                    n -= uint32_t(buf.size());
                }
            };
        }

        template<deserializable T>
            requires regular_optional_state<T>
        auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<optional<T>>) noexcept
        {
            return [](pmr_system_executor ex,event_source auto& source,optional<T>& x)
                    -> async_event_consumer {
                auto fer = first_event_repeater(ex,source);
                auto e = expect_event<optional<T>>(co_await fer,object_kind_set::any);
                if(e.kind()==object_kind::null)
                    x.reset();
                else{
                    if(!x)
                        x.emplace();
                    co_await serial_event_sink(type_tag<T>)(ex,fer,*x);
                }
            };
        }

        template<deserializable... Ts>
        auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<variant<Ts...>>) noexcept
        {
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return [](pmr_system_executor ex,event_source auto& source,variant<Ts...>& x)
                        -> async_event_consumer {
                    event e = expect_event<variant<Ts...>>(co_await source,{object_kind::sequence});
                    if(e.get_if<sequence_header>()->size!=2)
                        throw structure_error{structure_error::reason_t::out_of_range,
                                              boost::typeindex::type_id<variant<Ts...>>(),
                                              {object_kind::sequence},std::move(e)};
                    e = expect_event<variant<Ts...>>(co_await source,{object_kind::unsigned_int});
                    uint64_t i = *e.get_if<uint64_t>();
                    if(i>=sizeof...(Ts))
                        throw structure_error{structure_error::reason_t::out_of_range,
                                              boost::typeindex::type_id<variant<Ts...>>(),
                                              {object_kind::unsigned_int},std::move(e)};
                    static_cast<void>((...||(i==Indices&&(co_await serial_event_sink(type_tag<Ts>)
                        (ex,source,x.template emplace<Indices>()),true))));
                };
            }(std::index_sequence_for<Ts...>{});
        }

        template<deserializable_tuple_like T>
        auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<T>) noexcept
        {
            constexpr auto n = std::tuple_size_v<T>;
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return [](pmr_system_executor ex,event_source auto& source,T& x) -> async_event_consumer {
                    event e = expect_event<T>(co_await source,{object_kind::sequence});
                    if(e.get_if<sequence_header>()->size!=n)
                        throw structure_error{structure_error::reason_t::out_of_range,
                                            boost::typeindex::type_id<T>(),
                                            {object_kind::sequence},std::move(e)};
                    (...,(co_await serial_event_sink(type_tag<std::tuple_element_t<Indices,T>>)
                        (ex,source,get<Indices>(x))));
                };
            }(std::make_index_sequence<n>{});
        }

        template<container_like T>
            requires (!map_like<T>)&&(
                         overwritable_container_like<T>||
                         resizeable_container_like<T>||
                         emplace_back_container_like<T>||
                         set_container_like<T>)
        auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor ex,event_source auto& source,T& x) -> async_event_consumer {
                event e = expect_event<T>(co_await source,{object_kind::sequence});
                uint32_t s = e.get_if<sequence_header>()->size;
                using size_type = decltype(std::size(x));
                auto throw_out_of_range = [&]{
                    throw structure_error{structure_error::reason_t::out_of_range,
                                        boost::typeindex::type_id<T>(),
                                        {object_kind::unsigned_int},std::move(e)};
                };
                if(std::cmp_greater(s,std::numeric_limits<size_type>::max()))
                    throw_out_of_range();
                auto n = size_type(s);
                auto ses = serial_event_sink(type_tag<container_value_type_t<T>>);
                if constexpr(resizeable_container_like<T>||
                        (!emplace_back_container_like<T>&&!set_container_like<T>)){
                    if constexpr(resizeable_container_like<T>)
                        x.resize(n);
                    else if(n!=std::size(x))
                        throw_out_of_range();
                    for(auto& e:x)
                        co_await ses(ex,source,e);
                }else{
                    x.clear();
                    for(size_type i=0;i<n;++i)
                        if constexpr(emplace_back_container_like<T>)
                            co_await ses(ex,source,x.emplace_back());
                        else{
                            typename T::value_type v;
                            co_await ses(ex,source,v);
                            check_unique_key<T>(x.emplace(std::move(v)));
                        }
                }
            };
        }

        template<map_like T>
            requires deserializable<std::remove_const_t<map_like_key_type<T>>>&&
                    deserializable<map_like_mapped_type<T>>
        auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<T>) noexcept
        {
            return [](pmr_system_executor ex,event_source auto& source,T& x) -> async_event_consumer {
                event e = expect_event<T>(co_await source,{object_kind::map});
                uint32_t s = e.get_if<map_header>()->size;
                using size_type = decltype(std::size(x));
                if(std::cmp_greater(s,std::numeric_limits<size_type>::max()))
                    throw structure_error{structure_error::reason_t::out_of_range,
                                        boost::typeindex::type_id<T>(),
                                        {object_kind::unsigned_int},std::move(e)};
                auto n = size_type(s);
                using key_type = std::remove_const_t<map_like_key_type<T>>;
                using mapped_type = map_like_mapped_type<T>;
                auto sesk = serial_event_sink(type_tag<key_type>);
                auto sesv = serial_event_sink(type_tag<mapped_type>);
                x.clear();
                for(size_type i=0;i<n;++i){
                    key_type k;
                    co_await sesk(ex,source,k);
                    mapped_type v;
                    co_await sesv(ex,source,v);
                    check_unique_key<T>(x.emplace(std::move(k),std::move(v)));
                }
            };
        }
    }
}

#endif
