// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_D749618C_438A_4851_A09B_050461A6E526
#define UUID_D749618C_438A_4851_A09B_050461A6E526

#include <ampi/pmr/trivially_deallocatable_resource.hpp>
#include <ampi/utils/ref_counted_base.hpp>
#include <ampi/utils/tagged_pointer.hpp>

#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/memory_resource.hpp>

#include <cassert>
#include <concepts>
#include <memory>
#include <new>
#include <utility>

namespace ampi
{
    namespace detail
    {
        struct AMPI_EXPORT shared_memory_resource_base
            : public ref_counted_base<shared_memory_resource_base>
        {
            virtual ~shared_memory_resource_base();
            virtual boost::container::pmr::memory_resource* get() = 0;
        };

        template<typename T>
        struct shared_memory_resource : shared_memory_resource_base
        {
            T mr_;

            template<typename... Args>
            shared_memory_resource(Args&&... args)
                : mr_{std::forward<Args>(args)...}
            {}

            boost::container::pmr::memory_resource* get() override
            {
                return &mr_;
            }
        };

        class AMPI_EXPORT shared_polymorphic_allocator_base
        {
            constexpr static uintptr_t owning               = 0b01,
                                       trivial_deallocation = 0b10;
        public:
            shared_polymorphic_allocator_base() noexcept
                : p_{boost::container::pmr::get_default_resource()}
            {}

            template<std::derived_from<boost::container::pmr::memory_resource> T>
                requires (!std::is_const_v<T>)
            shared_polymorphic_allocator_base(T* mr) noexcept
                : p_{mr,trivial_deallocation*(
                     std::is_base_of_v<trivially_deallocatable_resource,T>||
                     is_trivially_deallocatable_resource(*mr))}
            {
                assert(mr);
            }

            template<typename T,typename... Args>
            shared_polymorphic_allocator_base(std::in_place_type_t<T>,Args&&... args)
                : p_{new shared_memory_resource<T>{std::forward<Args>(args)...},
                     owning+trivial_deallocation*
                         std::is_base_of_v<trivially_deallocatable_resource,T>}
            {}

            explicit shared_polymorphic_allocator_base(raw_pointer_tag_t,void* p) noexcept
                : p_{raw_pointer_tag,p}
            {}

            shared_polymorphic_allocator_base(const shared_polymorphic_allocator_base& other) noexcept
                : p_{other.p_}
            {
                if(p_.tag()&owning)
                    shared()->ref();
            }

            shared_polymorphic_allocator_base(shared_polymorphic_allocator_base&& other) noexcept
                : p_{std::exchange(other.p_,{})}
            {}

            shared_polymorphic_allocator_base& operator=(
                const shared_polymorphic_allocator_base& other) noexcept
            {
                if(this!=&other){
                    destroy();
                    p_ = other.p_;
                    if(p_.tag()&owning)
                        shared()->ref();
                }
                return *this;
            }

            shared_polymorphic_allocator_base& operator=(
                shared_polymorphic_allocator_base&& other) noexcept
            {
                if(this!=&other){
                    destroy();
                    p_ = std::exchange(other.p_,{});
                }
                return *this;
            }

            ~shared_polymorphic_allocator_base()
            {
                destroy();
            }

            void* to_raw_pointer() &&
            {
                void* ret = p_.to_raw_pointer();
                p_ = {};
                return ret;
            }

            explicit operator bool() const noexcept
            {
                return p_.pointer();
            }

            boost::container::pmr::memory_resource* resource() const noexcept
            {
                return p_.tag()&owning?shared()->get():non_owning();
            }

            bool is_trivially_deallocatable() const noexcept
            {
                return p_.tag()&trivial_deallocation;
            }

            [[nodiscard]] void* allocate_bytes(size_t bytes,
                                               size_t alignment = alignof(std::max_align_t))
            {
                return resource()->allocate(bytes,alignment);
            }

            void deallocate_bytes(void* p,size_t bytes,
                                  size_t alignment = alignof(std::max_align_t))
            {
                return resource()->deallocate(p,bytes,alignment);
            }

            template<typename U,typename... Args>
            void construct(U* p,Args&&... args)
            {
                // FIXME: no support in libc++.
                // std::uninitialized_construct_using_allocator(p,*this,std::forward<Args>(args)...);
                std::construct_at(p,std::forward<Args>(args)...);
            }

            template<typename U>
            [[nodiscard]] U* allocate_object(size_t n = 1)
            {
                if(size_t(-1)/sizeof(U)<n)
                    throw std::bad_array_new_length{};
                return static_cast<U*>(allocate_bytes(n*sizeof(U),alignof(U)));
            }

            template<typename U>
            void deallocate_object(U* p,size_t n = 1) noexcept
            {
                deallocate_bytes(p,n*sizeof(U),alignof(U));
            }

            template<typename U,typename... Args>
            [[nodiscard]] U* new_object(Args&&... args)
            {
                U* p = allocate_object<U>();
                try{
                    construct(p,std::forward<Args>(args)...);
                }
                catch(...){
                    deallocate_object(p);
                    throw;
                }
                return p;
            }

            template<typename U>
            void delete_object(U* p) noexcept
            {
                std::destroy_at(p);
                deallocate_object(p);
            }

            bool operator==(const shared_polymorphic_allocator_base& other) const noexcept = default;
        protected:
            tagged_pointer<void,2> p_ = {};
        private:
            boost::container::pmr::memory_resource* non_owning() const noexcept
            {
                return static_cast<boost::container::pmr::memory_resource*>(p_.pointer());
            }

            shared_memory_resource_base* shared() const noexcept
            {
                return static_cast<shared_memory_resource_base*>(p_.pointer());
            }

            void destroy() noexcept
            {
                if(p_.tag()&owning)
                    shared()->unref();
            }
        };
    }

    template<typename T = byte>
    class shared_polymorphic_allocator : public detail::shared_polymorphic_allocator_base
    {
    public:
        using value_type = T;

        using detail::shared_polymorphic_allocator_base::shared_polymorphic_allocator_base;

        shared_polymorphic_allocator(const detail::shared_polymorphic_allocator_base& other)
            : detail::shared_polymorphic_allocator_base{other}
        {}

        shared_polymorphic_allocator(detail::shared_polymorphic_allocator_base&& other)
            : detail::shared_polymorphic_allocator_base{std::move(other)}
        {}

        T* allocate(size_t n)
        {
            return static_cast<T*>(allocate_bytes(n*sizeof(T),alignof(T)));
        }

        void deallocate(T* p,size_t n)
        {
            return deallocate_bytes(p,n*sizeof(T),alignof(T));
        }
    };
}

#endif
