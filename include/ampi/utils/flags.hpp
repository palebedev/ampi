// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_F2D729EB_A595_4DFE_B977_417EAEBD947F
#define UUID_F2D729EB_A595_4DFE_B977_417EAEBD947F

#include <cassert>
#include <type_traits>

namespace ampi
{
    template<typename Enum>
    class flags
    {
        static_assert(std::is_enum_v<Enum>);
    public:
        using value_type = std::underlying_type_t<Enum>;

        constexpr flags() noexcept = default;

        constexpr flags(Enum e) noexcept
            : v_{static_cast<value_type>(e)}
        {}

        constexpr explicit flags(value_type v) noexcept
            : v_{v}
        {
            if constexpr(requires{ Enum::all_; })
                assert(!(v&~value_type(Enum::all_)));
        }

        constexpr explicit operator bool() const noexcept
        {
            return v_;
        }

        constexpr explicit operator value_type() const noexcept
        {
            return v_;
        }

        friend constexpr bool operator==(flags f1,flags f2) noexcept = default;

        friend constexpr flags operator|(flags f1,flags f2) noexcept
        {
            return flags{value_type(f1.v_|f2.v_)};
        }

        friend constexpr flags operator&(flags f1,flags f2) noexcept
        {
            return flags{value_type(f1.v_&f2.v_)};
        }

        friend constexpr flags operator^(flags f1,flags f2) noexcept
        {
            return flags{value_type(f1.v_^f2.v_)};
        }

        friend constexpr flags operator-(flags f1,flags f2) noexcept
        {
            return flags{value_type(f1.v_&~f2.v_)};
        }

        constexpr flags operator~() const noexcept
            requires requires { Enum::all_; }
        {
            return flags{value_type(value_type(Enum::all_)^v_)};
        }

        constexpr flags& operator|=(flags other) noexcept
        {
            v_ |= other.v_;
            return *this;
        }

        constexpr flags& operator&=(flags other) noexcept
        {
            v_ &= other.v_;
            return *this;
        }

        constexpr flags& operator^=(flags other) noexcept
        {
            v_ ^= other.v_;
            return *this;
        }

        constexpr flags& operator-=(flags other) noexcept
        {
            v_ &= ~other.v_;
            return *this;
        }
    private:
        value_type v_ = {};
    };
}

#endif
