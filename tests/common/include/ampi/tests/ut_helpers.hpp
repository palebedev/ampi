// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_26DDB987_D5D9_4C05_BB4D_E13C982C46FB
#define UUID_26DDB987_D5D9_4C05_BB4D_E13C982C46FB

#include <boost/algorithm/hex.hpp>
#include <boost/ut.hpp>

#include <algorithm>
#include <span>
#include <sstream>

namespace ampi
{
    constexpr inline struct : boost::ut::detail::op
    {
        constexpr operator std::nullptr_t() const noexcept
        {
            return nullptr;
        }
    } nullptr_v;

    class printable_binary_cspan_t : public std::span<const std::byte>
    {
    public:
        using std::span<const std::byte>::span;

        // Disable UT's per-element printing for containers.
        constexpr static size_t npos = size_t(-1);

        printable_binary_cspan_t(span<const std::byte> s) noexcept
            : std::span<const std::byte>{s}
        {}

        template<size_t N>
        printable_binary_cspan_t(const char (&s)[N]) noexcept
            : std::span<const std::byte>{reinterpret_cast<const std::byte*>(s),N-1}
        {}

        printable_binary_cspan_t(std::string_view sv) noexcept
            : printable_binary_cspan_t{reinterpret_cast<const std::byte*>(sv.data()),sv.size()}
        {}

        bool operator==(printable_binary_cspan_t other) noexcept
        {
            return std::equal(begin(),end(),other.begin(),other.end());
        }

        friend std::ostream& operator<<(std::ostream& stream,printable_binary_cspan_t pbc)
        {
            auto d = reinterpret_cast<const uint8_t*>(pbc.data());
            boost::algorithm::hex_lower(d,d+pbc.size(),std::ostream_iterator<char>{stream});
            return stream;
        }
    };
}

#endif
