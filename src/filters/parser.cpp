// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/filters/parser.hpp>

namespace ampi
{
    const char* parser_error::what() const noexcept
    {
        static const char* descriptions[] = {
            "leading byte 0xc1 is not used in MessagePack",
            "unexpected end of data",
            "invalid utf-8 data",
            "invalid timestamp length",
            "timestamp nanosecond > 999'999'999",
            "valid timestamp that is out of range for this implementation"
        };
        return descriptions[static_cast<int>(reason_)];
    }
}
