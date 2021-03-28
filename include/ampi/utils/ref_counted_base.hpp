// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_7A7328CE_22E7_4D11_B400_635A6773ACE8
#define UUID_7A7328CE_22E7_4D11_B400_635A6773ACE8

#include <ampi/utils/stdtypes.hpp>

#include <atomic>
#include <cassert>

namespace ampi
{
    template<typename Derived,size_t ReservedBits = 0>
    class ref_counted_base
    {
        static_assert(ReservedBits+16<sizeof(unsigned)*8);
    public:
        ref_counted_base(unsigned extra_data = 0) noexcept
            : refcount_{1u|(extra_data<<reserved_shift)}
        {
            assert(extra_data<=reserved_mask);
        }

        unsigned extra_data() const noexcept
        {
            return refcount_.load(std::memory_order::relaxed)>>reserved_shift;
        }

        void set_extra_data(unsigned extra_data) noexcept
        {
            assert(extra_data<=reserved_mask);
            unsigned v = refcount_.load(std::memory_order::relaxed);
            while(!refcount_.compare_exchange_weak(v,
                    (v&~reserved_mask)|(extra_data<<reserved_shift),
                    std::memory_order::release,
                    std::memory_order::relaxed));
        }

        void ref() noexcept
        {
            refcount_.fetch_add(1,std::memory_order::relaxed);
        }

        void unref()
        {
            if((refcount_.load(std::memory_order_relaxed)&~reserved_mask)!=1&&
                    (refcount_.fetch_sub(1,std::memory_order::release)&~reserved_mask)!=1)
                return;
            std::atomic_thread_fence(std::memory_order::acquire);
            delete static_cast<Derived*>(this);
        }
    private:
        constexpr static size_t reserved_shift = ReservedBits?sizeof(unsigned)*8-ReservedBits:0;
        constexpr static unsigned reserved_mask = ReservedBits?
            ((1u<<ReservedBits)-1u)<<reserved_shift:0;

        std::atomic<unsigned> refcount_;
    };

    struct ref_counted_base_deleter
    {
        template<typename Derived,size_t ReservedBits>
        void operator()(ref_counted_base<Derived,ReservedBits>* rcb) noexcept
        {
            rcb->unref();
        }
    };
}

#endif
