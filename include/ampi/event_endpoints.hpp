// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_5EA5C2F3_6B56_44C3_BBB4_57AB4B9A0DE5
#define UUID_5EA5C2F3_6B56_44C3_BBB4_57AB4B9A0DE5

#include <ampi/coro/coroutine.hpp>
#include <ampi/exception.hpp>
#include <ampi/execution/require.hpp>
#include <ampi/event.hpp>
#include <ampi/pmr/segmented_stack_resource.hpp>
#include <ampi/utils/tag_invoke.hpp>

#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/type_index.hpp>

#include <iterator>
#include <limits>
#include <tuple>

namespace ampi
{
    template<typename T>
    struct type_tag_t
    {
        using type = T;
    };
    
    template<typename T>
    constexpr inline type_tag_t<T> type_tag;

    using pmr_system_executor = require_result_t<
        boost::asio::system_executor,
        boost::asio::execution::allocator_t<boost::container::pmr::polymorphic_allocator<byte>>
    >;

    namespace detail
    {
        struct stack_executor_ctx
        {
            segmented_stack_resource mr;
            pmr_system_executor ex = boost::asio::require(boost::asio::system_executor{},
                boost::asio::execution::allocator(
                    boost::container::pmr::polymorphic_allocator<std::byte>{&mr}));
        };
    }

    class AMPI_EXPORT structure_error final : public exception
    {
    public:
        enum struct reason_t
        {
            unexpected_event,
            out_of_range,
            duplicate_key,
            unknown_key
        };

        structure_error(reason_t reason,boost::typeindex::type_index object_type,
                        object_kind_set expected_kinds = {},optional<event> event = {}) noexcept
            : reason_{reason},
              object_type_{object_type},
              expected_kinds_{expected_kinds},
              event_{std::move(event)}
        {}

        reason_t reason() const noexcept { return reason_; }
        boost::typeindex::type_index object_type() const noexcept { return object_type_; }
        object_kind_set expected_kinds() const noexcept { return expected_kinds_; }
        const optional<event>& event() const noexcept { return event_; }

        const char* what() const noexcept override;
    private:
        reason_t reason_;
        boost::typeindex::type_index object_type_;
        object_kind_set expected_kinds_;
        optional<class event> event_;
        mutable std::shared_ptr<std::string> what_;
    };

    namespace detail
    {
        template<typename T>
        struct is_optional : std::false_type {};

        template<typename T>
        struct is_optional<optional<T>> : std::true_type {};
    }

    template<typename T>
    concept regular_optional_state = (!std::is_same_v<std::nullptr_t,T>)&&
        (!detail::is_optional<T>{});

    template<typename T>
    concept tuple_like = requires {
        { std::tuple_size<T>::value } -> std::same_as<const size_t>;
    };

    template<typename T>
    struct container_value_type;

    template<typename T>
        requires requires { typename T::value_type; }
    struct container_value_type<T>
    {
        using type = typename T::value_type;
    };

    template<typename T>
        requires std::is_array_v<T>
    struct container_value_type<T>
    {
        using type = std::remove_extent_t<T>;
    };

    template<typename T>
    using container_value_type_t = typename container_value_type<T>::type;

    template<typename T>
    concept container_like = requires(const T& c) {
        { *std::begin(c) } -> std::convertible_to<typename container_value_type<T>::type>;
        std::end(c);
        { std::size(c) } -> integral;
    };

    template<typename T>
    concept map_like = container_like<T> && requires(T m,typename T::value_type v) {
        typename std::enable_if<std::tuple_size<typename T::value_type>::value==2>::type;
    };

    template<typename T>
    using map_like_key_type = std::tuple_element_t<0,typename T::value_type>;

    template<typename T>
    using map_like_mapped_type = std::tuple_element_t<1,typename T::value_type>;
}

#endif
