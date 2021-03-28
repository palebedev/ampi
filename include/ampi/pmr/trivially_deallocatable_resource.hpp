// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_956428C2_CC1D_422A_BE74_D049F86D277F
#define UUID_956428C2_CC1D_422A_BE74_D049F86D277F

#include <ampi/export.h>

#include <boost/container/pmr/memory_resource.hpp>

namespace ampi
{
    class AMPI_EXPORT trivially_deallocatable_resource : public boost::container::pmr::memory_resource
    {
    protected:
        void do_deallocate(void* p,size_t bytes,size_t alignment) override;
    };

    inline bool is_trivially_deallocatable_resource(boost::container::pmr::memory_resource& mr) noexcept
    {
        return dynamic_cast<trivially_deallocatable_resource*>(&mr);
    }
}

#endif
