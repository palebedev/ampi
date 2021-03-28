// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_77C929E8_9566_4F2C_91CF_F4A14A2732C7
#define UUID_77C929E8_9566_4F2C_91CF_F4A14A2732C7

#include <ampi/buffer_sources/async_stream_buffer_source.hpp>
#include <ampi/detail/msgunpack_ctx_base.hpp>

namespace ampi
{
    template<typename AsyncReadStream,readahead_t ReadAhead = readahead_t::available>
    class async_stream_msgunpack_ctx : public msgunpack_ctx_base_with_parser<
        async_generator<cbuffer,decltype(std::declval<AsyncReadStream>().get_executor())>>
    {
    public:
        explicit async_stream_msgunpack_ctx(AsyncReadStream& stream,
                shared_polymorphic_allocator<> spa = {},parser_options po = {},
                segmented_stack_resource ssr = {}) noexcept
            : msgunpack_ctx_base_with_parser<async_generator<cbuffer,
                      decltype(std::declval<AsyncReadStream>().get_executor())>>{
                  [&](pmr_buffer_factory& bf){
                      return async_stream_buffer_source<ReadAhead>(stream,bf);
                  },std::move(spa),po,std::move(ssr)}
        {}

        template<deserializable T,
                 boost::asio::completion_token_for<void (result<void>)> CompletionToken =
                     boost::asio::default_completion_token_t<
                         typename boost::asio::associated_executor<AsyncReadStream>::type>>
        auto async_msgunpack(T& x,CompletionToken&& token = {})
        {
            return [](executor auto,async_stream_msgunpack_ctx& this_,T& x)
                    -> coroutine<void,typename boost::asio::associated_executor<AsyncReadStream>::type> {
                auto p_v = this_.p();
                co_await serial_event_sink(type_tag<T>)(this_.ctx_.ex,p_v,x);
            }(this->p_.get_executor(),*this,x).
                async_run(std::forward<CompletionToken>(token));
        }
    };

    template<typename AsyncReadStream,deserializable T,
             boost::asio::completion_token_for<void (result<void>)> CompletionToken =
                boost::asio::default_completion_token_t<
                    typename boost::asio::associated_executor<AsyncReadStream>::type>>
    auto async_msgunpack(AsyncReadStream& stream,T& x,CompletionToken&& token = {})
    {
        return [](executor auto,AsyncReadStream& stream,T& x)
                 -> coroutine<void,typename boost::asio::associated_executor<AsyncReadStream>::type> {
            pmr_buffer_factory bf;
            detail::stack_executor_ctx ctx;
            auto asbs = async_stream_buffer_source<readahead_t::none>(stream,bf);
            parser p{asbs,bf,{},ctx.ex};
            auto p_v = p();
            co_await serial_event_sink(type_tag<T>)(ctx.ex,p_v,x);
        }(stream.get_executor(),stream,x).
            async_run(std::forward<CompletionToken>(token));
    }
}

#endif
