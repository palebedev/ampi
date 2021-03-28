// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/async_msgpack.hpp>
#include <ampi/async_msgunpack.hpp>
#include <ampi/event_sinks/hana_struct.hpp>
#include <ampi/event_sinks/pfr_tuple.hpp>
#include <ampi/event_sources/hana_struct.hpp>
#include <ampi/event_sources/pfr_tuple.hpp>
#include <ampi/istream.hpp>
#include <ampi/msgpack.hpp>
#include <ampi/ostream.hpp>
#include <ampi/transmute.hpp>
#include <ampi/value.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/static_thread_pool.hpp>
#include <boost/hana/adapt_struct.hpp>

#include <array>
#include <map>
#include <unordered_map>

namespace test
{
    struct aggregate_test
    {
        int a;
        double b;
    };

    class class_test
    {
    public:
        class_test(int a = 0,double b = 0.) noexcept : a_{a},b_{b} {}
        int a() const noexcept { return a_; }
        void set_a(int new_a) noexcept { a_ = new_a; }
        double b() const noexcept { return b_; }
        void set_b(double new_b) noexcept { b_ = new_b; }
    private:
        int a_;
        double b_;
    };
}
// These macros must be used at global scope.
BOOST_HANA_ADAPT_STRUCT(test::aggregate_test,a,b);
AMPI_ADAPT_CLASS(test::class_test,a,b);

int main()
{
    // AMPI is internally a set of coroutines that generate and consume
    // two types of objects: buffers (optionally owned memory spans) and events
    // (primitive components of decoded MessagePack data stream that don't depend
    // on its representation and buffers that carry variable length data types).
    // Implementing all data processing as coroutines internally allows AMPI
    // to transparanetly support synchronous and asynchronous use cases
    // without manually implementing complex state machines.
    // It's possible to use AMPI without touching any coroutine code in many cases.

    // AMPI provides the following buffer producers (sources) in "buffer_sources" subdir:
    // - one_buffer_source: yields a single buffer of in-memory data,
    // - istream_buffer_source: reads buffers from a std::istream,
    // - async_stream_buffer_source: reads buffers from ASIO AsyncReadStream.
    // and the following symmetric buffer consumers ("sinks", "buffer_sinks" subdir):
    // - container_buffer_sink: appends buffer data to a sequence-of-bytes container,
    // - ostream_buffer_sink: writes buffer data to std::ostream,
    // - async_stream_buffer_sink: collects batches of buffers and writes them
    //   to ASIO AsyncWriteStream.
    // It's possible to connect a span source and sink, which would work as a sort of
    // file copy, which is not too interseting, but would work.

    // The are two core coroutines that convert between streams of buffers and streams of
    // events ("filters", in same subdir):
    // - parser: converts stream of data buffers into stream of MessagePack events,
    // - emitter: converts stream of MessagePack events into stream of data buffers.
    // Connecting any buffer source to parser yields a pull parser that can be used manually
    // to parse MessagePack-encoded data without storing it, likewise, emitter + buffer source
    // gives a push-style MessagePack writer.
    // Unless you're working with MessagePack streams that won't fit in memory,
    // you likely want an even higher level interface that works with stored data.

    // AMPI prefers to use some data types in addition to or instead of counterparts in
    // the standard library. These are aliased in vocabulary.hpp.

    // AMPI uses tag_invoke (P1895R0) to provide customization points that associate data types
    // with event sources (serial_event_source) and sinks (serial_event_sinks).
    // AMPI provides built-in implementations for the following:
    // - std::nullptr_t maps to MessagePack NULL type.
    // - bool is BOOL.
    // - other integral types (excluding bool and non-narrow character types) map to
    //   SIGNED/UNSIGNED integer representations of required width.
    // - float/double are FLOAT and DOUBLE respectively.
    // - Anything that is convertible to string_view yields STRING.
    //   Specializations of basic_string for char type can consume STRINGs.
    // - optional of (de)serializable is a either its held value or NULL.
    // - variant is a sequence of INTEGER discriminator and the active member.
    // - Things that support tuple-like interface (std::tuple_size and friends) are
    //   SEQUENCES when all their members are (de)serializable (excluding arrays handled below).
    // - Anything that supports std::begin/end and has nested value_type (or element type
    //   for arrays) that is itself (de)serializable is considered a SEQUENCE.
    //   When deserializing a sequence, a container will be .resize()'d to needed size,
    //   if that is not possible, it will be .clear()'ed and then emplace_back()'ed (with 
    //   a call to .reserve() first, if present). If there's no emplace_back(), we try emplace()
    //   to support set-like things. If such an insert returns a tuple-like of size 2 with a second
    //   element of type bool, we consider this a unique-keyed container and check for duplicate
    //   keys. If there is neither resize nor clear+some kind of empalce, we just check current
    //   std::size() and overwite contents, failing if it doesn't match (to support std::array).
    // - If a range above has a tuple-like value_type of size 2, we treat it as a MAP.
    //   We use clear() + emplace(key,value) with same duplicate key check as above.
    // - If an object models boost::hana::Struct, it is treated as a MAP.
    //   We assume the keys are boost::hana::string's.
    // - As a last resort, if it's an aggregate that boost.PFR can reflect, we treat it as
    //   a SEQUENCE.
    // - timestamp_t (a specialization of std::chrono::time_point) is treated like a TIMESTAMP.
    
    // Connecting such source with a sink is called a "transmutation" that allows constructing
    // an object of same structure but possibly different type with the same contents through
    // serializing from one object and deserializing to another:
    {
        bool x = true,y;
        // transmute(Destination,Source) overwrites destination.
        // This is basically y = x, but more convoluted.
        ampi::transmute(y,x);
        assert(y);
    }
    {
        std::array<int,3> x = {1,2,3};
        // transmute<Destination>(Source) creates a default-constructed
        // object to deserialize to which is returned.
        // For the complex deserializers above, whenever an instance of the
        // element type is needed, it is value-initialized.
        // (This will be customizable in later versions.)
        // This works similar to std::copy between different value_types...
        auto y = ampi::transmute<ampi::vector<long>>(x);
        // ... but can actually handle differences many levels deep.
        struct S { int a; double b; bool c; };
        std::map<int,S> m = {{3,{4,5.6,false}},{-2,{5,8.1,true}}};
        auto um = ampi::transmute<std::unordered_map<long,std::tuple<int,double,bool>>>(m);
        assert(m.find(-2)->second.b==std::get<1>(um.find(-2)->second));
        // By including <ampi/event_{sink,source}/pfr_tuple.hpp> any aggregates that don't fall
        // into other concepts become (de)serializable as sequences of their elements, as if
        // they were tuples, which is why S above worked automatically.
    }

    // If you want classes to be represented as maps, you can make them
    // model boost::hana::Struct.
    // For classes without incapsulation, you can use BOOST_HANA_ADAPT_STRUCT or
    // BOOST_HANA_DEFINE_STRUCT to reflect their data member names (include respective
    // header file from Boost.Hana first).
    // If your class uses accessors for property "x" named "x" (getter) and "set_x" (setter),
    // and your setter takes its argument by value of the property type, you can use
    // AMPI_ADAPT_CLASS instead.
    // See definitions above main for usage.

    {
        test::aggregate_test at{-3,4.5};
        [[maybe_unused]] auto ct = ampi::transmute<test::class_test>(at);
        assert(at.a==ct.a()&&at.b==ct.b());
    }

    // AMPI provides a recursive variant-like type called 'value' that can hold any
    // MessagePack-like structure. For the arbitrary-length data types (string,binary,extension)
    // it uses a helper piecewise_view template that can hold either a container (string/vector)
    // or a vector of buffers represented as views (string_view,span<const byte>).
    // It allows preserving a sequence of buffers from original I/O without merging, which is
    // an extra data copy, while also storing a traditional container for the parts of the data
    // tree that are modified or constructed from scratch. It can be directly hashed/
    // compared against its view, iterated over as a sequence of views, or merge()'d into single
    // container. value can be written directly to an std::ostream to obtain a JSON-like human
    // readable representation. Transmutation allows its contents to be converted to and from
    // other serializable data structures.
    {
        std::map<int,std::unordered_map<int,int>> m;
        m[1] = {{2,3},{4,5}};
        m[6] = {{7,8}};
        auto v = ampi::transmute<ampi::value>(m);
        v.get_if<ampi::map>()->emplace(9,ampi::map{{10,11},{12,13}});
        std::ostringstream oss;
        oss << v << '\n';
        assert(std::move(oss).str()==std::string_view{+R"(
{
    1 : {
        2 : 3,
        4 : 5
    },
    6 : {
        7 : 8
    },
    9 : {
        10 : 11,
        12 : 13
    }
}
)"+1});
        // Write changes back to map.
        ampi::transmute(m,v);
    }

    // as_msgpack is a manipulator that takes a (de)serializable object and
    // can be read/written to a std::[io]stream to perform serialization/deserialization.
    {
        std::array<int,3> a = {1,2,3},b;
        std::stringstream ss;
        ss << ampi::as_msgpack(a);
        ss.seekg(0);
        ss >> ampi::as_msgpack(b);
        assert(a==b);
    }

    // async_msgpack/async_msgunpack do the same with ASIO Async streams:
    {
        namespace ba = boost::asio;
        ba::static_thread_pool pool{2};
        using executor_t = ba::static_thread_pool::executor_type;
        ba::basic_stream_socket<ba::local::stream_protocol,executor_t>
            from{pool.get_executor()},to{pool.get_executor()};
        ba::local::connect_pair(from,to);
        std::array<int,3> a = {1,2,3};
        ampi::async_msgpack(to,a,ba::bind_executor(pool.get_executor(),
            []([[maybe_unused]] ampi::result<void> e){
                assert(!e.has_error());
            }
        ));
        ampi::value v;
        ampi::async_msgunpack(from,v,ba::bind_executor(pool.get_executor(),
            []([[maybe_unused]] ampi::result<void> e){
                assert(!e.has_error());
            }
        ));
        pool.join();
        assert((*v.get_if<ampi::sequence>())[1]==2);
    }

    // msgunpack<T = value> combines one_span_buffer_source + parser + serial_event_sink:
    {
        const std::byte data[] = {std::byte{0x93},std::byte{0x01},std::byte{0x02},std::byte{0x03}};
        auto v = ampi::msgunpack({data,sizeof data});
        assert((*v.get_if<ampi::sequence>())[2]==3);
    }

    // msgpack combines serial_event_source + emitter + sequence_buffer_sink:
    {
        ampi::vector<std::byte> v = ampi::msgpack(42);
        assert(v.size()==1&&v[0]==std::byte{0x2a});
        v.clear();
        ampi::msgpack(v,std::set{-3,17});
        assert(v.size()==3&&v[0]==std::byte{0x92}&&
                            v[1]==std::byte{0xfd}&&
                            v[2]==std::byte{0x11});
    }

    // All types of (de)serialization - memory/iostream/asio stream-based
    // have advanced interfaces based on contexts. The contexts allow to reuse
    // some dynamically allocated resources in repeated operations and allow for more
    // customization:
    // - All contexts allow reuse of alternate coroutine stack which is dynamically allocated.
    // - All deserialization contexts allow setting of parser options.
    // - Deserialization contexts from streams allow customization of allocator used for buffer
    //   allocation. They also allow manual choice of read-ahead strategies:
    //   - 'none' uses hints from parser to always read the exact needed amount of characters
    //     on the current step. No extra memory is allocated and no trailing data can be captured
    //     in internal buffers, but a lot of small reads may be not as efficient.
    //     This is the only mode supported when not explicitly using the contexts.
    //   - 'available` asks the input device for amount of already available data at every read
    //     and a buffer of at least that size is requested from buffer factory. This leads to
    //     fewer bigger buffers allocated. To support trailing data from previous operation and
    //     extraction of trailing data after MessagePack, input contexts provide set_initial/rest
    //     methods.
    // Use of contexts should be preferred when you're using multiple I/O operations.
    // Write n MessagePack objects from in to out with correction.
    {
        auto f = [](size_t n,std::istream& in,std::ostream& out){
            // We want to return remaining buffer from this function, taken from the parser,
            // so that it outlives this block where its memory resource is created.
            // Normally we would use an-in_place_type constructor for
            // shared_polymorphic_allocator to make a shared ownership memory resource
            // which the buffer would share ownership with.
            // In this case since reusable_monotonic_buffer_resource derives from
            // trivially_deallocatable_resource, buffer does not store this
            // allocator to optimize its destruction, so it doesn't also share ownership.
            // Instead, use a normal non-owning shared_polymorphic_allocator construction
            // and explicitly take a copy of the value returned from rest().
            ampi::reusable_monotonic_buffer_resource mr;
            ampi::istream_msgpack_ctx<ampi::readahead_t::available> in_ctx{in,{&mr}};
            ampi::ostream_msgpack_ctx out_ctx{out};
            for(size_t i=0;i<n;++i){
                // Read any MessagePack message.
                // If it is a map with a 'user' key that contains 'john', change it to 'mark'.
                {
                    ampi::value v;
                    in_ctx >> v;
                    if(auto m = v.get_if<ampi::map>()){
                        auto it = m->find("user");
                        if(it!=m->end()&&it->second=="john")
                            it->second = "mark";
                    }
                    // Write to output.
                    out_ctx << v;
                }
                // v has been destroyed, but the memory is left in the arena. Reset it for reuse.
                mr.reuse();
            }
            // Return copy of any extra data left to caller.
            return ampi::buffer{std::move(in_ctx).rest(),ampi::shared_polymorphic_allocator<>{}};
        };
        // Prepare input data:
        std::stringstream in,out;
        {
            ampi::ostream_msgpack_ctx out_ctx{in};
            // 1. MessagePack object that is not a map.
            out_ctx << 42
            // 2. What we want to change.
                    << ampi::value{{{"abc","def"},{"user","john"}}};
        }
        // 3. Trailing data that is not message pack.
        std::byte trailing[] = {std::byte{0xc1},std::byte{0x00},std::byte{0xc1}};
        in.write(reinterpret_cast<const char*>(trailing),sizeof trailing);
        // Run processing.
        in.seekg(0);
        auto rest = f(2,in,out);
        // Check that modifications were made.
        assert(std::move(out).str()==std::string_view{
            // 42
            "\x2a"
            // map of 2 key-values
            "\x82"
                // "abc":"def"
                "\xa3""abc" "\xa3""def"
                // "user":"mark"
                "\xa4""user" "\xa4""mark"
        });
        // And the rest can be any part of what went after in input.
        assert(rest.size()<=sizeof trailing&&
               std::equal(rest.begin(),rest.end(),trailing));
    }
}
