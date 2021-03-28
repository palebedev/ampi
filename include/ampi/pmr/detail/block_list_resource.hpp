// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_C8EA7B98_B92A_4C3C_BA66_F4282AEF1CC7
#define UUID_C8EA7B98_B92A_4C3C_BA66_F4282AEF1CC7

#include <ampi/utils/stdtypes.hpp>

#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/memory_resource.hpp>

#include <algorithm>
#include <bit>
#include <cassert>
#include <memory>
#include <new>
#include <utility>

namespace ampi::detail
{
    struct block_header
    {
        alignas(boost::container::pmr::memory_resource::max_align) size_t size_;
        block_header* next_ = nullptr;

        block_header(block_header* /*prev*/,size_t size) noexcept
            : size_{size}
        {}
    };

    class block_list_resource_base
    {
    public:
        constexpr static size_t default_initial_size = 128;

        explicit block_list_resource_base(size_t initial_size = default_initial_size)
            : block_list_resource_base{boost::container::pmr::get_default_resource(),initial_size}
        {}

        explicit block_list_resource_base(boost::container::pmr::memory_resource* upstream,
                size_t initial_size = default_initial_size)
            : upstream_{upstream},
              initial_size_{initial_size}
        {
            assert(upstream);
        }

        boost::container::pmr::memory_resource* upstream() const noexcept
        {
            return upstream_;
        }
    protected:
        boost::container::pmr::memory_resource* upstream_;
        size_t initial_size_;
    };

    template<typename Derived,typename Base = boost::container::pmr::memory_resource,
             typename BlockHeader = block_header>
    class block_list_resource : public Base,public block_list_resource_base
    {
    public:
        using block_list_resource_base::block_list_resource_base;

        block_list_resource(block_list_resource&& other) noexcept
            : block_list_resource_base{other},
              first_{std::exchange(other.first_,{})},
              current_{std::exchange(other.first_,{})}
        {}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
        ~block_list_resource()
#pragma clang diagnostic pop
        {
            free_blocks();
        }

        size_t remaining_storage(size_t alignment,size_t& wasted_due_to_alignment) const noexcept
        {
            if(!current_){
                wasted_due_to_alignment = 0;
                return 0;
            }
            void* p = this_().current_p();
            size_t remaining = left();
            void* old_p = p;
            if(!std::align(alignment,1,p,remaining)){
                wasted_due_to_alignment = remaining;
                return 0;
            }
            wasted_due_to_alignment = size_t(static_cast<const byte*>(p)-
                                             static_cast<const byte*>(old_p));
            return remaining-wasted_due_to_alignment;
        }

        size_t remaining_storage(size_t alignment = 1) const noexcept
        {
            size_t wasted;
            return remaining_storage(alignment,wasted);
        }
    protected:
        BlockHeader *first_ = nullptr,
                    *current_ = nullptr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
        void* do_allocate(size_t bytes,size_t alignment)
#pragma clang diagnostic pop
        {
            if(alignment>this->max_align)
                throw std::bad_alloc{};
            void* p = nullptr;
            if(current_){
                p = this_().current_p();
                size_t remaining = left();
                p = std::align(alignment,bytes,p,remaining);
                if(!p)
                    while(current_->next_){
                        current_ = static_cast<BlockHeader*>(current_->next_);
                        if(bytes<=(current_->size_-sizeof(BlockHeader))){
                            p = reinterpret_cast<byte*>(current_)+sizeof(BlockHeader);
                            break;
                        }
                    }
            }
            if(!p){
                constexpr static size_t address_space = size_t(-1),
                                        half_address_space = address_space/2;
                size_t s = current_?(current_->size_<half_address_space?current_->size_*2:address_space):
                                    initial_size_,
                       need = bytes+sizeof(BlockHeader);
                s = std::max(s,need<half_address_space?std::bit_ceil(need):address_space);
                auto n = static_cast<BlockHeader*>(upstream_->allocate(s));
                new (static_cast<void*>(n)) BlockHeader{current_,s};
                if(auto old = std::exchange(current_,n))
                    old->next_ = n;
                else
                    first_ = current_;
                p = reinterpret_cast<byte*>(n)+sizeof(BlockHeader);
            }
            this_().set_current_p(static_cast<byte*>(p)+bytes);
            return p;
        }
        
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
        bool do_is_equal(const boost::container::pmr::memory_resource& other) const noexcept
#pragma clang diagnostic pop
        {
            return this==dynamic_cast<const Derived*>(&other);
        }

        void free_blocks()
        {
            auto c = first_;
            while(c){
                auto old = std::exchange(c,static_cast<BlockHeader*>(c->next_));
                upstream_->deallocate(old,old->size_);
            }
        }

        size_t left() const noexcept
        {
            return current_->size_-size_t(static_cast<byte*>(this_().current_p())-
                                          reinterpret_cast<byte*>(current_));
        }
    private:
        Derived& this_() noexcept
        {
            return static_cast<Derived&>(*this);
        }

        const Derived& this_() const noexcept
        {
            return static_cast<const Derived&>(*this);
        }
    };
}

#endif
