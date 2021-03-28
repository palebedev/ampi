// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_5C1C8DC8_78DE_47A6_9417_5751BA71D719
#define UUID_5C1C8DC8_78DE_47A6_9417_5751BA71D719

#include <ampi/event_endpoints.hpp>

#include <boost/callable_traits/args.hpp>
#include <boost/hana/accessors.hpp>
#include <boost/hana/size.hpp>
#include <boost/hana/string.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/size.hpp>

namespace ampi::detail
{
    template<typename Class,typename T,auto Getter,auto Setter>
    struct accessor_proxy
    {
        using proxied_value_type = T;

        Class instance_;

        operator T() const noexcept
        {
            return (instance_.*Getter)();
        }

        accessor_proxy& operator=(T x) noexcept
        {
            (instance_.*Setter)(std::move(x));
            return *this;
        }
    };

#define AMPI_ADAPT_CLASS_MEMBER_IMPL(class_,name)                      \
    boost::hana::make_pair(                                            \
        BOOST_HANA_STRING(BOOST_PP_STRINGIZE(name)),                   \
        [](auto&& instance){                                           \
            return ampi::detail::accessor_proxy<                       \
                decltype(instance)&&,                                  \
                std::tuple_element_t<1,boost::callable_traits::args_t< \
                    decltype(&class_::BOOST_PP_CAT(set_,name))>>,      \
                &class_::name,                                         \
                &class_::BOOST_PP_CAT(set_,name)                       \
            >{std::forward<decltype(instance)>(instance)};             \
        }                                                              \
    )                                                                  \
    /**/

#define AMPI_ADAPT_CLASS_MEMBER(z,i,tuple)         \
    AMPI_ADAPT_CLASS_MEMBER_IMPL(                  \
        BOOST_PP_TUPLE_ELEM(0,tuple),              \
        BOOST_PP_TUPLE_ELEM(BOOST_PP_INC(i),tuple) \
    )                                              \
    /**/

#define AMPI_ADAPT_CLASS(class_,...)                              \
    template<>                                                    \
    struct boost::hana::accessors_impl<class_>                    \
    {                                                             \
        constexpr static auto apply()                             \
        {                                                         \
            return boost::hana::make_tuple(                       \
                BOOST_PP_ENUM(BOOST_PP_TUPLE_SIZE((__VA_ARGS__)), \
                              AMPI_ADAPT_CLASS_MEMBER,            \
                              (class_,__VA_ARGS__))               \
            );                                                    \
        }                                                         \
    }                                                             \
    /**/

    template<typename T>
    struct accessor_result_impl
    {
        using type = std::remove_cvref_t<T>;
    };

    template<typename T>
        requires requires { typename T::proxied_value_type; }
    struct accessor_result_impl<T>
    {
        using type = typename T::proxied_value_type;
    };

    template<typename T>
    struct accessor_result : detail::accessor_result_impl<T> {};

    template<typename T>
    using accessor_result_t = typename accessor_result<T>::type;
}

#endif
