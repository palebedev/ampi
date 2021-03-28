// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_991A7F4E_9F52_4F1D_AE26_0C1C12F648F2
#define UUID_991A7F4E_9F52_4F1D_AE26_0C1C12F648F2

#include <ampi/pmr/shared_polymorphic_allocator.hpp>

#include <boost/container/vector.hpp>
#include <boost/outcome/boost_result.hpp>
#include <boost/variant2/variant.hpp>

#include <exception>
#include <optional>
#include <span>
#include <string>

namespace ampi
{
    using std::optional;

    // No valueless_by_exception, has subset.
    using boost::variant2::variant;
    using boost::variant2::get_if;
    using boost::variant2::holds_alternative;
    using boost::variant2::in_place_type;
    using boost::variant2::visit;

    template<typename T>
    using result = boost::outcome_v2::boost_result<T,std::exception_ptr>;

    // We want pmr and our allocator.
    using string = std::basic_string<char,std::char_traits<char>,shared_polymorphic_allocator<char>>;
    using std::string_view;

    // We want pmr, default_init is useful.
    template<typename T>
    using vector = boost::container::vector<T,shared_polymorphic_allocator<T>>;

    using std::span;
    using binary_view_t = span<byte>;
    using binary_cview_t = span<const byte>;
}

#endif
