// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_E9A6C78D_E1CE_44CD_AB9F_10833086C33B
#define UUID_E9A6C78D_E1CE_44CD_AB9F_10833086C33B

#include <ampi/coro/coroutine.hpp>
#include <ampi/piecewise_view.hpp>
#include <ampi/utils/compare.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/outcome/boost_result.hpp>

#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>

namespace ampi
{
    static_assert(std::numeric_limits<float>::is_iec559&&sizeof(float)==4,
                  "float is not IEEE 754 single precision floating point type");
    static_assert(std::numeric_limits<double>::is_iec559&&sizeof(double)==8,
                  "double is not IEEE 754 double precision floating point type");

    enum struct object_kind : uint8_t
    {
        null,
        bool_,
        unsigned_int,
        // Only negative values, positive signed are passed as unsigned.
        signed_int,
        float_,
        double_,
        // sequence=map+1 is used in parsing 0x80-0x9f and 0xdc-0xdf
        map,
        sequence,
        // extension=binary+1 is used in parsing 0xc4-0xc9
        // binary-string is a range that is read in chunks is used in read_bytes
        binary,
        extension,
        string,
        timestamp,
        data_buffer,
        kind_count_
    };

    AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,object_kind kind);

    class AMPI_EXPORT object_kind_set
    {
        struct any_tag {};
    public:
        const static object_kind_set any;

        template<typename... Kinds>
            requires (...&&std::is_same_v<Kinds,object_kind>)
        constexpr object_kind_set(Kinds... kinds) noexcept
        {
            (...,(set_ |= 1u<<static_cast<size_t>(kinds)));
        }

        constexpr explicit operator bool() const noexcept { return set_; }

        friend constexpr object_kind_set operator&(object_kind_set oks1,object_kind_set oks2)
        {
            object_kind_set result;
            result.set_ = oks1.set_&oks2.set_;
            return result;
        }

        friend constexpr object_kind_set operator|(object_kind_set oks1,object_kind_set oks2)
        {
            object_kind_set result;
            result.set_ = oks1.set_|oks2.set_;
            return result;
        }

        friend constexpr object_kind_set operator^(object_kind_set oks1,object_kind_set oks2)
        {
            object_kind_set result;
            result.set_ = oks1.set_^oks2.set_;
            return result;
        }

        friend constexpr object_kind_set operator-(object_kind_set oks1,object_kind_set oks2)
        {
            object_kind_set result;
            result.set_ = oks1.set_&~oks2.set_;
            return result;
        }

        friend std::ostream& operator<<(std::ostream& stream,object_kind_set oks);
    private:
        unsigned set_ = 0u;

        static_assert(static_cast<size_t>(object_kind::kind_count_)<sizeof(set_)*8);

        constexpr object_kind_set(any_tag) noexcept
            : set_{(1u<<static_cast<size_t>(object_kind::kind_count_))-1u}
        {}
    };

    const inline object_kind_set object_kind_set::any = object_kind_set::any_tag{};

    template<typename T>
    concept integral = std::integral<T>&&
                       (!std::is_same_v<T,bool>)&&
                       (!std::is_same_v<T,char8_t>)&&
                       (!std::is_same_v<T,char16_t>)&&
                       (!std::is_same_v<T,char32_t>)&&
                       (!std::is_same_v<T,wchar_t>);

    template<typename T>
    concept floating_point = std::is_same_v<T,float>||std::is_same_v<T,double>;

    template<typename T>
    concept arithmetic = integral<T>||floating_point<T>;

    using timestamp_t = std::chrono::sys_time<std::chrono::nanoseconds>;

    struct sequence_header
    {
        uint32_t size;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::strong_ordering operator<=>(const sequence_header&) const = default;
#pragma clang diagnostic pop
    };

    struct map_header
    {
        uint32_t size;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::strong_ordering operator<=>(const map_header&) const = default;
#pragma clang diagnostic pop
    };

    struct binary_header
    {
        uint32_t size;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::strong_ordering operator<=>(const binary_header&) const = default;
#pragma clang diagnostic pop
    };

    struct extension_header
    {
        uint32_t size;
        int8_t type;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::strong_ordering operator<=>(const extension_header&) const = default;
#pragma clang diagnostic pop
    };

    struct string_header
    {
        uint32_t size;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::strong_ordering operator<=>(const string_header&) const = default;
#pragma clang diagnostic pop
    };

    AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,timestamp_t ts);

    namespace detail
    {
        struct event_variant_equal_t
        {
            template<typename T,typename U>
            bool operator()(const T&,const U&) const noexcept
            {
                return false;
            }

            template<typename T>
                requires (!arithmetic<T>)
            bool operator()(const T& x,const T& y) const noexcept
            {
                return x==y;
            }

            template<typename T,typename U>
                requires arithmetic<T>&&arithmetic<U>
            bool operator()(T x,U y) const noexcept
            {
                if constexpr(integral<T>&&integral<U>)
                    return std::cmp_equal(x,y);
                else{
                    using common_t = std::common_type_t<T,U>;
                    if(auto o = common_t(x)<=>common_t(y);o!=std::partial_ordering::unordered)
                        return o==std::partial_ordering::equivalent;
                    return std::isnan(x)==std::isnan(y);
                }
            }

            bool operator()(const cbuffer& b1,const cbuffer& b2) const noexcept
            {
                return std::equal(b1.begin(),b1.end(),b2.begin(),b2.end());
            }
        };

        template<typename Variant>
        bool variant_equal(const Variant& v1,const Variant& v2)
        {
            return &v1==&v2||visit(event_variant_equal_t{},v1,v2);
        }

        template<typename Range>
        std::strong_ordering lexicographical_compare_three_way(const Range& r1,const Range& r2)
        {
            // FIXME: no std::lexicographical_compare_three_way.
            auto it1=r1.begin(),it2=r2.begin(),e1=r1.end(),e2=r2.end();
            for(;it1!=e1&&it2!=e2;++it1,++it2)
                if(auto o = *it1<=>*it2;o!=std::strong_ordering::equal)
                    return o;
            return it1!=e1?std::strong_ordering::greater:
                   it2!=e2?std::strong_ordering::less:
                           std::strong_ordering::equal;
        }

        struct event_variant_three_way_t
        {
            size_t i1,i2;

            template<typename T,typename U>
            std::strong_ordering operator()(const T&,const U&) const noexcept
            {
                return i1<=>i2;
            }

            template<typename T>
                requires (!arithmetic<T>)
            std::strong_ordering operator()(const T& x,const T& y) const noexcept
            {
                return x<=>y;
            }

            std::strong_ordering operator()(std::nullptr_t,std::nullptr_t) const noexcept
            {
                return std::strong_ordering::equal;
            }

            template<typename T,typename U>
                requires arithmetic<T>&&arithmetic<U>
            std::strong_ordering operator()(T x,U y) const noexcept
            {
                if constexpr(integral<T>&&integral<U>)
                    return std::cmp_equal(x,y)?std::strong_ordering::equal:
                           std::cmp_less(x,y)?std::strong_ordering::less:
                                              std::strong_ordering::greater;
                else{
                    using common_t = std::common_type_t<T,U>;
                    if(auto o = common_t(x)<=>common_t(y);o!=std::partial_ordering::unordered)
                        return make_strong_ordering(o);
                    return std::isnan(x)<=>std::isnan(y);
                }
            }

            std::strong_ordering operator()(timestamp_t t1,timestamp_t t2) const noexcept
            {
                // FIXME: no operator<=> for std::chrono::time_point.
                return t1==t2?std::strong_ordering::equal:
                       t1<t2 ?std::strong_ordering::less:
                              std::strong_ordering::greater;
            }

            std::strong_ordering operator()(const cbuffer& b1,const cbuffer& b2) const noexcept
            {
                return lexicographical_compare_three_way(b1,b2);
            }
        };

        template<typename ThreeWay = event_variant_three_way_t,typename Variant>
        std::strong_ordering variant_three_way(const Variant& v1,const Variant& v2)
        {
            return &v1==&v2?std::strong_ordering::equal:
                            visit(ThreeWay{v1.index(),v2.index()},v1,v2);
        }
    }

    class event
    {
        variant<
            std::nullptr_t,bool,uint64_t,int64_t,float,double,
            map_header,sequence_header,binary_header,extension_header,string_header,
            timestamp_t,cbuffer
        > v_;

        template<typename T>
        constexpr static bool is_event_type_v = boost::mp11::mp_find<decltype(v_),T>{}<
                                                boost::mp11::mp_size<decltype(v_)>{};
    public:
        event(std::convertible_to<decltype(v_)> auto x)
            : v_{std::move(x)}
        {}

        object_kind kind() const noexcept
        {
            return static_cast<object_kind>(v_.index());
        }

        template<typename T>
            requires is_event_type_v<T>
        T* get_if() noexcept
        {
            return ampi::get_if<T>(&v_);
        }

        template<typename T>
            requires is_event_type_v<T>
        const T* get_if() const noexcept
        {
            return ampi::get_if<T>(&v_);
        }

        auto get_atomic_events() && noexcept
        {
            return std::move(v_).subset<std::nullptr_t,bool,uint64_t,int64_t,
                                        float,double,timestamp_t>();
        }

        friend bool operator==(const event& e1,const event& e2) noexcept
        {
            return detail::variant_equal(e1.v_,e2.v_);
        }

        friend std::strong_ordering operator<=>(const event& e1,const event& e2) noexcept
        {
            return detail::variant_three_way(e1.v_,e2.v_);
        }

        friend AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,const event& e);
    };

    template<typename T>
    concept event_source = async_generator_yielding<T,event>;

    constexpr inline size_t max_msgpack_fixed_buffer_length = 1+1+1+4+8; // timestamp 96
}

#endif
