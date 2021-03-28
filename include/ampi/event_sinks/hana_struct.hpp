// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_7B932D1F_C39D_43A5_9121_C225BC83858F
#define UUID_7B932D1F_C39D_43A5_9121_C225BC83858F

#include <ampi/detail/hana_struct.hpp>
#include <ampi/event_sinks/event_sink.hpp>

#include <bitset>
#include <exception>

namespace ampi::serial_event_sink_ns
{
    namespace detail
    {
        template<typename T>
        concept deserializable_accessor = deserializable<ampi::detail::accessor_result_t<T>> && requires(T x) {
            x = std::declval<ampi::detail::accessor_result_t<T>>();
        };

        template<typename T>
            requires boost::hana::Struct<T>::value
        constexpr bool deserializable_hana_struct() noexcept
        {
            auto a = boost::hana::accessors<T>();
            constexpr size_t n = boost::hana::size(a);
            if(n>0xffffffff)
                return false;
            return []<size_t... Indices>(std::index_sequence<Indices...>){
                return (...&&deserializable_accessor<
                    decltype(boost::hana::second(a[boost::hana::size_c<Indices>])
                    (std::declval<T>()))>);
            }(std::make_index_sequence<n>{});
        }
    }

    template<typename T>
    concept deserializable_hana_struct = detail::deserializable_hana_struct<T>();

    template<deserializable_hana_struct T>
    auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<T>) noexcept
    {
        return []<size_t... Indices>(std::index_sequence<Indices...>){
            return [](pmr_system_executor ex,event_source auto& source,T& x) -> async_event_consumer {
                auto a = boost::hana::accessors<T>();
                event e = expect_event<T>(co_await source,{object_kind::map});
                uint32_t n = e.get_if<map_header>()->size;
                constexpr size_t s = boost::hana::size(a);
                if(n>s)
                    throw structure_error{structure_error::reason_t::out_of_range,
                                          boost::typeindex::type_id<T>(),
                                          {object_kind::sequence},std::move(e)};
                std::bitset<s> seen;
                auto matched_name = [&](auto i,string_view name){
                    if(name!=string_view{boost::hana::first(a[i]).c_str()})
                        return false;
                    if(seen[i])
                        throw structure_error{structure_error::reason_t::duplicate_key,
                                              boost::typeindex::type_id<T>()};
                    seen[i] = true;
                    return true;
                };
                auto get_ses = [&](auto i){
                    decltype(auto) v = boost::hana::second(a[i])(x);
                    using result_t = ampi::detail::accessor_result_t<decltype(v)>;
                    if constexpr(std::is_same_v<std::remove_cvref_t<decltype(v)>,result_t>)
                        return serial_event_sink(type_tag<result_t>)(ex,source,v);
                    else{
                        auto ses = serial_event_sink(type_tag<result_t>);
                        using sink_t = decltype(ses(ex,source,std::declval<result_t&>()));
                        using awaiter_t = decltype(std::declval<sink_t>().operator co_await());
                        using wrapper_t = awaiter_wrapper<awaiter_t>;
                        struct setter_base
                        {
                            result_t x_;
                            sink_t sink_;

                            setter_base(decltype(ses) ses,pmr_system_executor ex,
                                        decltype(source) source)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
                                : sink_{ses(ex,source,x_)}
#pragma clang diagnostic pop
                            {}
                        };
                        struct setter : setter_base,wrapper_t
                        {
                            decltype(v) v_;
                            int uc_ = std::uncaught_exceptions();

                            setter(decltype(ses) ses,pmr_system_executor ex,decltype(source) source,
                                   decltype(v) v_)
                                : setter_base{std::move(ses),ex,source},
                                  wrapper_t{std::move(this->sink_).operator co_await()},
                                  v_{v_}
                            {}

                            ~setter() noexcept(noexcept(v_ = std::declval<result_t>()))
                            {
                                if(uc_==std::uncaught_exceptions())
                                    v_ = std::move(this->x_);
                            }
                        };
                        return setter{std::move(ses),ex,source,std::move(v)};
                    }
                };
                auto str_ses = serial_event_sink(type_tag<string>);
                auto mr = boost::asio::query(ex,boost::asio::execution::allocator_t<void>{}).resource();
                for(uint32_t i=0;i<n;++i){
                    async_event_consumer name_coro;
                    string name{{mr}};
                    name_coro = str_ses(ex,source,name);
                    co_await std::move(name_coro);
                    if(!(...||(matched_name(boost::hana::size_c<Indices>,name)&&
                           (co_await get_ses(boost::hana::size_c<Indices>),true))))
                        throw structure_error{structure_error::reason_t::unknown_key,
                                              boost::typeindex::type_id<T>(),{},std::move(e)};
                }
            };
        }(std::make_index_sequence<boost::hana::size(boost::hana::accessors<T>())>{});
    }
}

#endif
