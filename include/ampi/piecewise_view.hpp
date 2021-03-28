// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_B9560858_2AF8_477A_97E4_064EB10A3E2A
#define UUID_B9560858_2AF8_477A_97E4_064EB10A3E2A

#include <ampi/buffer.hpp>
#include <ampi/hash/span.hpp>
#include <ampi/utils/ref_counted_base.hpp>

#include <boost/container/pmr/small_vector.hpp>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <cassert>
#include <compare>
#include <iosfwd>

namespace ampi
{
    template<typename View,typename Container>
    class piecewise_view
    {
        struct iterator_transform_t
        {
            View operator()(const cbuffer& buf) const
            {
                return {reinterpret_cast<typename View::const_pointer>(buf.data()),buf.size()};
            }
        };
    public:
        using piece_vector_t = boost::container::small_vector<
            cbuffer,2,shared_polymorphic_allocator<cbuffer>>;
        using iterator = boost::iterators::transform_iterator<iterator_transform_t,const cbuffer*>;

        piecewise_view() noexcept = default;

        piecewise_view(Container cont) noexcept
            : v_{single_t{std::move(cont)}}
        {}

        piecewise_view(View view) noexcept
            : piecewise_view{cbuffer{{reinterpret_cast<const byte*>(view.data()),view.size()}}}
        {}

        piecewise_view(cbuffer buf) noexcept
            : piecewise_view{std::move(buf),buf.allocator()?
                  shared_polymorphic_allocator<cbuffer>{*buf.allocator()}:
                  shared_polymorphic_allocator<cbuffer>{}
              }
        {}

        piecewise_view(cbuffer buf,shared_polymorphic_allocator<cbuffer> spa)
            : piecewise_view{piece_vector_t{{std::move(buf)},
                 // Explicitly cast needed because small_vector is only
                 // constructible from its special small_vector_allocator,
                 // not Allocator template parameter, and that special allocator's
                 // constructor from Allocator is explicit.
                                            piece_vector_t::allocator_type{std::move(spa)}}}
        {}

        piecewise_view(piece_vector_t pv) noexcept
            : v_{std::move(pv)}
        {}

        [[nodiscard]] bool empty() const noexcept
        {
            return !size();
        }

        size_t size() const noexcept
        {
            if(auto single = get_if<single_t>(&v_))
                return single->cont_.size();
            return piece_size();
        }

        Container merge(typename Container::allocator_type alloc = {}) const&
        {
            if(auto single = get_if<single_t>(&v_))
                return Container{single->cont_,alloc};
            return merge_impl(std::move(alloc));
        }

        Container merge(typename Container::allocator_type alloc = {}) &&
        {
            if(auto single = get_if<single_t>(&v_))
                return Container{std::move(single->cont_),alloc};
            return merge_impl(std::move(alloc));
        }

        iterator begin() const noexcept
        {
            if(auto single = get_if<single_t>(&v_))
                return {&single->cont_span_,{}};
            return {get_if<piece_vector_t>(&v_)->data(),{}};
        }

        iterator end() const noexcept
        {
            if(auto single = get_if<single_t>(&v_))
                return {&single->cont_span_+1,{}};
            auto& pv = *get_if<piece_vector_t>(&v_);
            return {pv.data()+pv.size(),{}};
        }

        bool operator==(const piecewise_view& other) const noexcept
        {
            return size()==other.size()&&*this<=>other==std::strong_ordering::equal;
        }

        std::strong_ordering operator<=>(const piecewise_view& other) const noexcept
        {
            return cmp_3way(other.begin(),other.end());
        }

        bool operator==(View other) const noexcept
        {
            return size()==other.size()&&*this<=>other==std::strong_ordering::equal;
        }

        std::strong_ordering operator<=>(View other) const noexcept
        {
            return cmp_3way(&other,&other+1);
        }
    private:
        struct single_t
        {
            Container cont_;
            cbuffer cont_span_;

            explicit single_t(Container cont) noexcept
                : cont_{std::move(cont)},
                  cont_span_{{reinterpret_cast<const byte*>(cont_.data()),cont_.size()}}
            {}
        };

        variant<single_t,piece_vector_t> v_;

        size_t piece_size() const noexcept
        {
            std::size_t s = 0;
            for(auto& buf:*get_if<piece_vector_t>(&v_))
                s += buf.size();
            return s;
        }

        Container merge_impl(typename Container::allocator_type alloc) const
        {
            Container cont;
            if(auto p = get_if<piece_vector_t>(&v_)){
                Container cont{std::move(alloc)};
                cont.reserve(piece_size());
                for(auto& buf:*p){
                    auto d = reinterpret_cast<const typename Container::value_type*>(buf.data());
                    cont.insert(cont.end(),d,d+buf.size());
                }
                return cont;
            }else{
                auto& cont = *get_if<Container>(&v_);
                assert(alloc!=cont.get_allocator());
                return Container{std::move(cont),std::move(alloc)};
            }
        }

        std::strong_ordering cmp_3way(auto i2,auto e2) const noexcept
        {
            // FIXME: libc++ doesn't provide std::ranges::join.
            auto i1 = begin(),e1 = end();
            View v1,v2;
            if(i1!=e1)
                v1 = *i1;
            if(i2!=e2)
                v2 = *i2;
            for(;;){
                if(i1==e1)
                    return i2==e2?std::strong_ordering::equal:std::strong_ordering::less;
                if(i2==e2)
                    return std::strong_ordering::greater;
                auto n = std::max(v1.size(),v2.size());
                // FIXME: missing std::lexicographical_compare_three_way
                for(std::size_t i=0;i<n;++i)
                    if(auto o = v1[i]<=>v2[i];o!=std::strong_ordering::equal)
                        return o;
                if(n==v1.size()&&++i1!=e1)
                    v1 = *i1;
                if(n==v2.size()&&++i2!=e2)
                    v2 = *i2;
            }
        }

        friend size_t hash_value(const piecewise_view& pv) noexcept
        {
            return boost::hash_range(pv.begin(),pv.end());
        }
    };

    using piecewise_string = piecewise_view<string_view,string>;
    using piecewise_data = piecewise_view<binary_cview_t,vector<byte>>;

    AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,const piecewise_string& ps);
    AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,const piecewise_data& pd);

    namespace detail
    {
        struct quoted_piecewise_string
        {
            const piecewise_string& ps;
            char delim,escape;
        };

        AMPI_EXPORT std::ostream& operator<<(std::ostream& stream,const quoted_piecewise_string& qps);
    }

    inline auto quoted(const piecewise_string& ps,char delim='"',char escape='\\') noexcept
    {
        return detail::quoted_piecewise_string{ps,delim,escape};
    }
}

template<typename View,typename Container>
struct std::hash<ampi::piecewise_view<View,Container>>
{
    std::size_t operator()(const ampi::piecewise_view<View,Container>& pv) const noexcept
    {
        return hash_value(pv);
    }
};

#endif
