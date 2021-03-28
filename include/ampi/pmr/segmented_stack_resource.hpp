// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_51873227_EA61_4C9D_B269_1598BEF7415E
#define UUID_51873227_EA61_4C9D_B269_1598BEF7415E

#include <ampi/export.h>
#include <ampi/pmr/detail/block_list_resource.hpp>

namespace ampi
{
    namespace detail
    {
        struct bidi_block_header : block_header
        {
            bidi_block_header* prev_;
            void* p_;

            bidi_block_header(bidi_block_header* prev,size_t size) noexcept
                : block_header{prev,size},
                  prev_{prev}
            {}
        };
    }

    class AMPI_EXPORT segmented_stack_resource final
        : public detail::block_list_resource<segmented_stack_resource,
                                             boost::container::pmr::memory_resource,
                                             detail::bidi_block_header>
    {
    public:
        using base_t = detail::block_list_resource<segmented_stack_resource,
            boost::container::pmr::memory_resource,detail::bidi_block_header>;

        using base_t::base_t;

        segmented_stack_resource(segmented_stack_resource&& other) noexcept = default;

        ~segmented_stack_resource() override;
    protected:
        void do_deallocate(void* p,size_t bytes,size_t alignment) override;
    private:
        friend base_t;

        void* current_p() const noexcept
        {
            return current_->p_;
        }

        void set_current_p(void* new_p) noexcept
        {
            current_->p_ = new_p;
        }
    };
}

#endif
