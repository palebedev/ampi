// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_28D47F28_1CBE_46E3_BED8_D48CE3E6B9AC
#define UUID_28D47F28_1CBE_46E3_BED8_D48CE3E6B9AC

#include <sstream>

namespace ampi
{
    template<typename... Ts>
    std::string format(Ts&&... args)
    {
        std::ostringstream oss;
        (oss << ... << std::forward<Ts>(args));
        return std::move(oss).str();
    }
}

#endif
