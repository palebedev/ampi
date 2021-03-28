// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_93D50884_4E2D_48CC_9BBC_5C668A9CAD60
#define UUID_93D50884_4E2D_48CC_9BBC_5C668A9CAD60

#include <ampi/buffer_sinks/ostream_buffer_sink.hpp>
#include <ampi/detail/fixed_msgpack_buffer_factory.hpp>
#include <ampi/detail/msgpack_ctx_base.hpp>
#include <ampi/event_sources/event_source.hpp>
#include <ampi/filters/emitter.hpp>
#include <ampi/manipulator.hpp>

#include <ostream>

namespace ampi
{
    class ostream_msgpack_ctx : public msgpack_ctx_base
    {
    public:
        explicit ostream_msgpack_ctx(std::ostream& stream,segmented_stack_resource ssr = {}) noexcept
            : msgpack_ctx_base{std::move(ssr)},
              stream_{&stream}
        {}

        template<serializable T>
        ostream_msgpack_ctx& operator<<(const T& x)
        {
            auto ses = serial_event_source(type_tag<T>)(ctx_.ex,x);
            detail::fixed_msgpack_buffer_factory bf;
            auto em = emitter(ctx_.ex,ses,bf);
            auto sink = ostream_buffer_sink(ctx_.ex,*stream_,em).assume_blocking();
            sink();
            return *this;
        }
    private:
        std::ostream* stream_;
    };

    namespace detail
    {
        template<typename T>
            requires serializable<std::remove_const_t<T>>
        std::ostream& operator<<(std::ostream& stream,as_msgpack_t<T> amt)
        {
            ostream_msgpack_ctx(stream) << amt.x;
            return stream;
        }
    }
}

#endif
