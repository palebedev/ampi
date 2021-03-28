// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_2FDAA7EA_A48D_4BAD_B5B7_A60108498A10
#define UUID_2FDAA7EA_A48D_4BAD_B5B7_A60108498A10

#include <ampi/buffer_sinks/async_stream_buffer_sink.hpp>
#include <ampi/detail/msgpack_ctx_base.hpp>
#include <ampi/event_sources/event_source.hpp>
#include <ampi/filters/emitter.hpp>
#include <ampi/pmr/reusable_monotonic_buffer_resource.hpp>

namespace ampi
{
    namespace detail
    {
        template<typename IOVec>
        class reusing_iovec_t : public IOVec
        {
        public:
            reusing_iovec_t(IOVec iovec,reusable_monotonic_buffer_resource& mbr)
                : IOVec{std::move(iovec)},
                  mbr_{mbr}
            {}

            void clear() noexcept
            {
                IOVec::clear();
                mbr_.reuse();
            }
        private:
            reusable_monotonic_buffer_resource& mbr_;
        };
    }

    template<typename AsyncWriteStream,typename IOVec = default_iovec_t>
    class async_stream_msgpack_ctx : public msgpack_ctx_base
    {
    public:
        explicit async_stream_msgpack_ctx(AsyncWriteStream& stream,segmented_stack_resource ssr = {},
                IOVec iovec = {}) noexcept
            : msgpack_ctx_base{std::move(ssr)},
              stream_{&stream},
              iovec_{iovec,mbr_}
        {}

        template<serializable T,
                 boost::asio::completion_token_for<void (result<void>)> CompletionToken =
                 boost::asio::default_completion_token_t<
                     typename boost::asio::associated_executor<AsyncWriteStream>::type>>
        auto async_msgpack(const T& x,CompletionToken&& token = {})
        {
            mbr_.reuse();
            return [](executor auto,async_stream_msgpack_ctx& this_,const T& x)
                    -> coroutine<void,typename boost::asio::associated_executor<AsyncWriteStream>::type> {
                auto ses = serial_event_source(type_tag<T>)(this_.ctx_.ex,x);
                auto em = emitter(this_.ctx_.ex,ses,this_.bf_);
                co_await async_stream_buffer_sink(*this_.stream_,em,this_.iovec_);
            }(stream_->get_executor(),*this,x).async_run(std::forward<CompletionToken>(token));
        }
    private:
        AsyncWriteStream* stream_;
        reusable_monotonic_buffer_resource mbr_;
        pmr_buffer_factory bf_{&mbr_};
        detail::reusing_iovec_t<IOVec> iovec_;
    };

    template<typename AsyncWriteStream,serializable T,
             boost::asio::completion_token_for<void (result<void>)> CompletionToken =
             boost::asio::default_completion_token_t<
                 typename boost::asio::associated_executor<AsyncWriteStream>::type>>
    auto async_msgpack(AsyncWriteStream& stream,const T& x,CompletionToken&& token = {})
    {
        return [](executor auto,AsyncWriteStream& stream,const T& x)
                -> coroutine<void,typename boost::asio::associated_executor<AsyncWriteStream>::type> {
            detail::stack_executor_ctx ctx;
            auto ses = serial_event_source(type_tag<T>)(ctx.ex,x);
            reusable_monotonic_buffer_resource mr;
            pmr_buffer_factory bf{&mr};
            auto em = emitter(ctx.ex,ses,bf);
            detail::reusing_iovec_t<default_iovec_t> iovec{{},mr};
            co_await async_stream_buffer_sink(stream,em,iovec);
        }(stream.get_executor(),stream,x).async_run(std::forward<CompletionToken>(token));
    }
}

#endif
