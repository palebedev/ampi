// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_AAF4E171_A2F9_4892_8578_A9CDFF078116
#define UUID_AAF4E171_A2F9_4892_8578_A9CDFF078116

#include <ampi/detail/msgpack_ctx_base.hpp>
#include <ampi/event_sinks/event_sink.hpp>
#include <ampi/filters/parser.hpp>

namespace ampi
{
    class msgunpack_ctx_base : public msgpack_ctx_base
    {
    public:
        explicit msgunpack_ctx_base(parser_options po = {},segmented_stack_resource ssr = {}) noexcept
            : msgpack_ctx_base{std::move(ssr)},
              po_{po}
        {}
    protected:
        parser_options po_;
    };

    template<typename BufferSource>
    class msgunpack_ctx_base_with_parser : public msgunpack_ctx_base
    {
    public:
        template<typename BufferSourceFactory>
        msgunpack_ctx_base_with_parser(BufferSourceFactory bsf,
                shared_polymorphic_allocator<> spa,parser_options po,
                segmented_stack_resource ssr) noexcept
            : msgunpack_ctx_base{po,std::move(ssr)},
              bf_{std::move(spa)},
              bs_{bsf(bf_)},
              p_{bs_,bf_,po,ctx_.ex}
        {}

        void set_initial(cbuffer buf) noexcept
        {
            p_.set_initial(std::move(buf));
        }

        cbuffer rest() noexcept
        {
            return p_.rest();
        }
    protected:
        pmr_buffer_factory bf_;
        BufferSource bs_;
        parser<BufferSource,pmr_buffer_factory,pmr_system_executor> p_;
    };
}

#endif
