// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_49537D96_1C5A_4CAC_985E_5DE7C3FA0D59
#define UUID_49537D96_1C5A_4CAC_985E_5DE7C3FA0D59

#include <ampi/vocabulary.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <cassert>
#include <new>
#include <span>
#include <utility>

namespace ampi
{
    namespace detail
    {
        struct buffer_header : public boost::intrusive_ref_counter<buffer_header>
        {
            size_t capacity_;
            shared_polymorphic_allocator<> spa_;

            buffer_header(size_t n,shared_polymorphic_allocator<>&& spa) noexcept
                : capacity_{n},
                  spa_{std::move(spa)}
            {}

            void operator delete(buffer_header* p,std::destroying_delete_t)
            {
                auto spa = std::move(p->spa_);
                // After move of spa, destruction is trivial.
                // p->~buffer_header();
                spa.deallocate_bytes(p,sizeof(buffer_header)+p->capacity_);
            }
        };
    }

    class cbuffer
    {
    public:
        cbuffer() noexcept = default;

        cbuffer(const cbuffer& other) noexcept = default;
        cbuffer& operator=(const cbuffer& other) noexcept = default;

        cbuffer(cbuffer&& other) noexcept
            : header_{std::move(other.header_)},
              view_{std::exchange(other.view_,{})}
        {}

        cbuffer& operator=(cbuffer&& other) noexcept
        {
            if(this!=&other){
                header_ = std::move(other.header_);
                view_ = std::exchange(other.view_,{});
            }
            return *this;
        }

        cbuffer(binary_cview_t bv) noexcept
            : header_{},
              view_{const_cast<std::byte*>(bv.data()),bv.size()}
        {}

        cbuffer(cbuffer& other,size_t offset,size_t count = std::dynamic_extent) noexcept
            : header_{other.header_},
              view_{other.view_.subspan(offset,count)}
        {}

        cbuffer(cbuffer&& other,size_t offset,size_t count = std::dynamic_extent) noexcept
            : header_{std::move(other.header_)},
              view_{other.view_.subspan(offset,count)}
        {
            other.view_ = {};
        }

        const shared_polymorphic_allocator<>* allocator() const noexcept
        {
            return header_?&header_->spa_:nullptr;
        }

        explicit operator bool() const noexcept
        {
            return view_.size();
        }

        size_t size() const noexcept
        {
            return view_.size();
        }

        const byte* data() const noexcept
        {
            return view_.data();
        }

        binary_cview_t view() const noexcept
        {
            return view_;
        }

        operator boost::asio::const_buffer() const noexcept
        {
            return {data(),size()};
        }

        byte operator[](size_t i) const noexcept
        {
            return view_[i];
        }

        const byte* begin() const noexcept
        {
            return view_.data();
        }

        const byte* end() const noexcept
        {
            return view_.data()+view_.size();
        }

        bool append(cbuffer& other) noexcept
        {
            if(header_&&other.header_)
                return false;
            if(!other)
                return true;
            if(!*this){
                *this = std::move(other);
                return true;
            }
            if(end()!=other.begin())
                return false;
            view_ = {view_.data(),view_.size()+other.view_.size()};
            other.view_ = {};
            if(!header_)
                header_ = std::move(other.header_);
            return true;
        }

        friend std::ostream& operator<<(std::ostream& stream,const cbuffer& buf)
        {
            boost::algorithm::hex_lower(reinterpret_cast<const uint8_t*>(buf.begin()),
                                        reinterpret_cast<const uint8_t*>(buf.end()),
                                        std::ostream_iterator<char>{stream});
            return stream;
        }
    protected:
        boost::intrusive_ptr<detail::buffer_header> header_;
        binary_view_t view_;

        cbuffer(boost::intrusive_ptr<detail::buffer_header> header) noexcept
            : header_{std::move(header)},
              view_{reinterpret_cast<byte*>(header_.get())+sizeof(detail::buffer_header),
                    header_->capacity_}
        {}
    };

    class buffer : public cbuffer
    {
    public:
        constexpr static size_t default_size = 4*1024*1024;

        buffer() noexcept = default;

        buffer(binary_view_t bv) noexcept
            : cbuffer{bv}
        {}

        explicit buffer(size_t n,shared_polymorphic_allocator<> spa = {})
            : buffer{spa.is_trivially_deallocatable()?
                  buffer{{static_cast<byte*>(spa.allocate_bytes(n)),n}}:
                  buffer{::new (spa.allocate_bytes(sizeof(detail::buffer_header)+n,
                      alignof(detail::buffer_header))) detail::buffer_header{n,std::move(spa)}}}
        {
            assert(n);
        }

        buffer(binary_cview_t bv,shared_polymorphic_allocator<> spa)
            : buffer{bv.size(),std::move(spa)}
        {
            assert(!bv.empty());
            std::memcpy(data(),bv.data(),bv.size());
        }

        buffer(buffer& other,size_t offset,size_t count = std::dynamic_extent) noexcept
            : cbuffer{other,offset,count}
        {}

        buffer(buffer&& other,size_t offset,size_t count = std::dynamic_extent) noexcept
            : cbuffer{std::move(other),offset,count}
        {}

        using cbuffer::data;

        byte* data() const noexcept
        {
            return view_.data();
        }

        using cbuffer::view;

        binary_view_t view() noexcept
        {
            return view_;
        }

        operator boost::asio::mutable_buffer() noexcept
        {
            return {data(),size()};
        }

        using cbuffer::operator[];

        byte& operator[](size_t i) noexcept
        {
            return view_[i];
        }

        using cbuffer::begin;

        byte* begin() noexcept
        {
            return view_.data();
        }

        using cbuffer::end;

        byte* end() noexcept
        {
            return view_.data()+view_.size();
        }

        bool append(buffer& other) noexcept
        {
            return cbuffer::append(other);
        }
    private:
        buffer(boost::intrusive_ptr<detail::buffer_header> header) noexcept
            : cbuffer{header}
        {}
    };

    template<typename T>
    concept buffer_factory = requires(T& bf,size_t n) {
        { bf.get_buffer() } -> std::convertible_to<buffer>;
        { bf.get_buffer(n) } -> std::convertible_to<buffer>;
        bf.next_buffer_size(n);
    };

    class null_buffer_factory
    {
    public:
        buffer get_buffer(size_t /*n*/ = 0) noexcept
        {
            BOOST_UNREACHABLE_RETURN(buffer{});
        }

        void next_buffer_size(size_t /*n*/) noexcept {}
    };

    class pmr_buffer_factory
    {
    public:
        constexpr static size_t default_default_buffer_size = 0x80000;

        explicit pmr_buffer_factory(shared_polymorphic_allocator<> spa,
                size_t default_buffer_size = default_default_buffer_size) noexcept
            : spa_{std::move(spa)},
              default_buffer_size_{default_buffer_size}
        {}

        explicit pmr_buffer_factory(size_t default_buffer_size = default_default_buffer_size)
                noexcept
            : pmr_buffer_factory{{},default_buffer_size}
        {}

        buffer get_buffer(size_t size = 0)
        {
            size_t n = std::max(size,std::exchange(next_buffer_size_,0));
            return buffer{n?n:default_buffer_size_,spa_};
        }

        void next_buffer_size(size_t n) noexcept
        {
            next_buffer_size_ = n;
        }
    private:
        shared_polymorphic_allocator<> spa_;
        size_t default_buffer_size_,next_buffer_size_ = 0;
    };
}

#endif
