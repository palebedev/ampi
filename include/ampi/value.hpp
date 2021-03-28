// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_3E6FEBBC_9311_4E08_B97B_7F7D2835D2B5
#define UUID_3E6FEBBC_9311_4E08_B97B_7F7D2835D2B5

#include <ampi/export.h>
#include <ampi/event_sinks/event_sink.hpp>
#include <ampi/event_sources/event_source.hpp>
#include <ampi/hash/flat_map.hpp>
#include <ampi/hash/time_point.hpp>
#include <ampi/hash/vector.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace ampi
{
    class value;

    using sequence = vector<value>;
    using map = boost::container::flat_map<value,value,std::less<>,
                                           shared_polymorphic_allocator<std::pair<value,value>>>;

    struct extension
    {
        int8_t type;
        piecewise_data data;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::strong_ordering operator<=>(const extension& other) const noexcept = default;
#pragma clang diagnostic pop
    };

    inline size_t hash_value(const extension& e) noexcept
    {
        size_t seed = 0;
        boost::hash_combine(seed,e.type);
        boost::hash_combine(seed,e.data);
        return seed;
    }

    AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,const extension& ext);

    namespace detail
    {
        struct AMPI_EXPORT value_variant_three_way_t : event_variant_three_way_t
        {
            value_variant_three_way_t(size_t index1,size_t index2) noexcept
                : event_variant_three_way_t{index1,index2}
            {}

            using event_variant_three_way_t::operator();

            std::strong_ordering operator()(std::nullptr_t,std::nullptr_t) const noexcept
            {
                return std::strong_ordering::equal;
            }

            std::strong_ordering operator()(const sequence& s1,const sequence& s2) const noexcept;
            std::strong_ordering operator()(const map& s1,const map& s2) const noexcept;
        };

        struct AMPI_EXPORT value_se_source
        {
            delegating_event_generator operator()(pmr_system_executor ex,const value& v) const;
        };

        struct value_se_sink
        {
            async_event_consumer operator()(pmr_system_executor ex,event_source auto& source,value& v) const;
        };
    }

    class AMPI_EXPORT value
    {
        struct print_visitor;

        friend detail::value_se_source;
        friend detail::value_se_sink;

        variant<std::nullptr_t,bool,uint64_t,int64_t,float,double,map,sequence,
                piecewise_data,extension,piecewise_string,timestamp_t> v_;

        template<typename T>
        constexpr static bool is_value_type_v = boost::mp11::mp_contains<decltype(v_),T>{};
    public:
        value(std::nullptr_t = {}) noexcept
        {}

        value(bool v) noexcept
            : v_{v}
        {}

        template<integral T>
        value(T v)
            : v_{[v](){
                     if constexpr(std::signed_integral<T>)
                         return int64_t(v);
                     else
                         return uint64_t(v);
                 }()}
        {}

        value(float v) noexcept
            : v_{v}
        {}

        value(double v) noexcept
            : v_{v}
        {}

        value(sequence seq) noexcept
            : v_{std::move(seq)}
        {}

        value(map m) noexcept
            : v_{std::move(m)}
        {}

        value(piecewise_data pd) noexcept
            : v_{std::move(pd)}
        {}

        value(extension e) noexcept
            : v_{std::move(e)}
        {}

        value(piecewise_string ps) noexcept
            : v_{std::move(ps)}
        {}

        value(const char* s) noexcept
            : v_{string_view{s}}
        {}

        value(timestamp_t t) noexcept
            : v_{t}
        {}

        object_kind kind() const noexcept
        {
            return object_kind(v_.index());
        }

        template<typename T>
            requires is_value_type_v<T>
        const T* get_if() const
        {
            return ampi::get_if<T>(&v_);
        }

        template<typename T>
            requires is_value_type_v<T>
        T* get_if()
        {
            return ampi::get_if<T>(&v_);
        }

        friend bool operator==(const value& v1,const value& v2) noexcept
        {
            return detail::variant_equal(v1.v_,v2.v_);
        }

        friend std::strong_ordering operator<=>(const value& v1,const value& v2) noexcept
        {
            return detail::variant_three_way<detail::value_variant_three_way_t>(v1.v_,v2.v_);
        }

        friend size_t hash_value(const value& v) noexcept
        {
            return hash_value(v.v_);
        }

        friend AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,const value& v);

        friend auto tag_invoke(tag_t<serial_event_source>,type_tag_t<value>) noexcept
        {
            return detail::value_se_source{};
        }

        friend auto tag_invoke(tag_t<serial_event_sink>,type_tag_t<value>) noexcept
        {
            return detail::value_se_sink{};
        }
    };

    namespace detail
    {
        async_event_consumer value_se_sink::operator()(pmr_system_executor ex,
            event_source auto& source,value& v) const
        {
            auto fer = serial_event_sink_ns::first_event_repeater(ex,source);
            event e = serial_event_sink_ns::expect_event<value>(co_await fer,
                object_kind_set::any-object_kind::data_buffer);
            switch(e.kind()){
                case object_kind::map:
                    if(v.kind()!=object_kind::map)
                        v.v_ = map{};
                    co_await serial_event_sink(type_tag<map>)(ex,fer,*get_if<map>(&v.v_));
                    break;
                case object_kind::sequence:
                    if(v.kind()!=object_kind::sequence)
                        v.v_ = sequence{};
                    co_await serial_event_sink(type_tag<sequence>)
                        (ex,fer,*get_if<sequence>(&v.v_));
                    break;
                case object_kind::binary:
                case object_kind::extension:
                case object_kind::string:
                    {
                        uint32_t n = e.kind()==object_kind::binary?e.get_if<binary_header>()->size:
                                     e.kind()==object_kind::string?e.get_if<string_header>()->size:
                                     e.get_if<extension_header>()->size;
                        piecewise_data::piece_vector_t pieces;
                        while(n){
                            cbuffer buf = *serial_event_sink_ns::expect_event<piecewise_data>(
                                co_await source,{object_kind::data_buffer}).
                                    template get_if<cbuffer>();
                            n -= uint32_t(buf.size());
                            pieces.emplace_back(std::move(buf));
                        }
                        if(e.kind()==object_kind::binary)
                            v.v_ = piecewise_data{std::move(pieces)};
                        else if(e.kind()==object_kind::string)
                            v.v_ = piecewise_string{std::move(pieces)};
                        else
                            v.v_ = extension{e.get_if<extension_header>()->type,
                                             piecewise_data{std::move(pieces)}};
                    }
                    break;
                default:
                    v.v_ = std::move(e).get_atomic_events();
            }
        }
    }
}

template<>
struct std::hash<ampi::extension>
{
    std::size_t operator()(const ampi::extension& e) const noexcept
    {
        return hash_value(e);
    }
};

template<>
struct std::hash<ampi::value>
{
    std::size_t operator()(const ampi::value& v) const noexcept
    {
        return hash_value(v);
    }
};

#endif
