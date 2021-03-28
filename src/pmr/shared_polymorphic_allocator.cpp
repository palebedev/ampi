// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/pmr/shared_polymorphic_allocator.hpp>

namespace ampi::detail
{
    shared_memory_resource_base::~shared_memory_resource_base() = default;
}
