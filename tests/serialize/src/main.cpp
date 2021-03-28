// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/tests/ut_helpers.hpp>

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

#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/hana/adapt_struct.hpp>

#include <map>
#include <set>
#include <unordered_map>

struct hana_test
{
    bool abc;
    int def;
    ampi::string ghijkl;

    bool operator==(const hana_test& other) const = default;
};
BOOST_HANA_ADAPT_STRUCT(hana_test,abc,def,ghijkl);

struct hana_test2
{
    float xyz;
};
BOOST_HANA_ADAPT_STRUCT(hana_test2,xyz);

class hana_test3
{
public:
    explicit hana_test3(int x = 0): x_{x} {}
    int x() const noexcept { return x_; }
    void set_x(int new_x) noexcept { x_ = new_x; }
    bool operator==(const hana_test3& other) const = default;
private:
    int x_;
};

AMPI_ADAPT_CLASS(hana_test3,x);

struct pfr_test
{
    std::string x;
    unsigned y;

    bool operator==(const pfr_test& other) const = default;
};

namespace ampi { namespace
{
    using namespace boost::ut;

    template<typename To,typename From>
    void test_roundtrip(const From& x)
    {
        To y = transmute<To>(x);
        expect(x==y);
    }

    template<typename T>
    void test_roundtrip(const T& x)
    {
        test_roundtrip<T,T>(x);
    }

    template<typename To,typename From>
    void test_roundtrip_integral(const From& x)
    {
        To y = transmute<To>(x);
        expect(std::cmp_equal(x,y));
    }

    suite transmutation = []{
        "primitive"_test = []{
            test_roundtrip(nullptr);
            test_roundtrip(true);
            test_roundtrip(uint8_t(1));
            test_roundtrip(uint16_t(2));
            test_roundtrip(uint32_t(3));
            test_roundtrip(uint64_t(4));
            test_roundtrip(int8_t(-1));
            test_roundtrip(int16_t(-2));
            test_roundtrip(int32_t(-3));
            test_roundtrip(int64_t(-4));
            test_roundtrip(1.5f);
            test_roundtrip(4.2);

            test_roundtrip_integral<int>(uint16_t(2));
            expect(throws<structure_error>([]{
                test_roundtrip_integral<unsigned>(-3);
            }));
            expect(throws<structure_error>([]{
                transmute<int8_t>(0xffffu);
            }));

            {
                using namespace std::chrono_literals;
                test_roundtrip(timestamp_t{std::chrono::sys_days{2020y/10/15}+18h+40min+36s});
            }

            {
                test_roundtrip(std::string{"test"});
                test_roundtrip<string>("abcdefghijklmnopqrstuvwxyz");
            }

            test_roundtrip(vector<byte>{byte{0x01},byte{0x02},byte{0x03}});
        };
        "optional"_test = []{
            test_roundtrip(optional<int>{});
            test_roundtrip(optional<double>{3.45});
        };
        "variant"_test = []{
            test_roundtrip(variant<bool,int,double>{42});
        };
        "tuple"_test = []{
            test_roundtrip(std::pair<bool,int>{true,42});
            test_roundtrip(std::tuple<>{});
            test_roundtrip(std::tuple<bool,int,double>{true,42,1.23});
        };
        "sequence"_test = []{
            {
                int a[3] = {1,2,3},b[3];
                transmute(b,a);
                expect(std::equal(std::begin(a),std::end(a),std::begin(b),std::end(b)));
            }
            test_roundtrip(std::array<int,0>{});
            test_roundtrip(std::array<int,3>{1,2,3});
            test_roundtrip(vector<int>{1,2,3});
            test_roundtrip(std::set<int>{1,2,3});
            test_roundtrip(std::multiset<int>{1,2,2,3});
            expect(throws<structure_error>([]{
                transmute<std::set<int>>(std::multiset<int>{1,2,2,3});
            }));
        };
        "map"_test = []{
            test_roundtrip(std::map<int,double>{{1,2.3},{4,5.6}});
            test_roundtrip(std::unordered_multimap<int,double>{{1,2.3},{1,4.5},{6,7.8}});
            expect(throws<structure_error>([]{
                transmute<std::unordered_map<int,double>>(
                    std::unordered_multimap<int,double>{{1,2.3},{1,4.5},{6,7.8}});
            }));
        };
        "hana"_test = []{
            test_roundtrip(hana_test{true,45,"test"});
            expect(throws<structure_error>([]{
                transmute<hana_test>(hana_test2{});
            }));
            test_roundtrip(hana_test3{42});
        };
        "pfr"_test = []{
            test_roundtrip(pfr_test{"test",123u});
        };
        "value"_test = []{
            test_roundtrip(value{1.23});
            test_roundtrip(value{sequence{}});
            test_roundtrip(value{{1,2,3}});
            test_roundtrip(value{{{{"abc",true},{"defg",-3}}}});
        };
    };

    template<typename T>
    void test_io(const T& x,string_view s)
    {
        if constexpr(std::is_same_v<T,event>){
            {
                auto oss = one_buffer_source({reinterpret_cast<const byte*>(s.data()),s.size()});
                null_buffer_factory bf;
                detail::stack_executor_ctx ctx;
                parser p{oss,bf,{},ctx.ex};
                auto p_v = p().assume_blocking();
                auto e = p_v();
                expect(e!=nullptr_v);
                if(e)
                    expect(that%x==*e);
            }
            {
                detail::stack_executor_ctx ctx;
                auto ses = [](pmr_system_executor,event e) -> noexcept_event_generator {
                    co_yield std::move(e);
                }(ctx.ex,x);
                detail::fixed_msgpack_buffer_factory bf;
                auto em = emitter(ctx.ex,ses,bf).assume_blocking();
                auto buf = em();
                expect(buf!=nullptr_v);
                if(buf)
                    expect(that%printable_binary_cspan_t{s}==printable_binary_cspan_t{buf->view()});
                expect(em()==nullptr_v);
            }
        }else{
            std::stringstream ss{std::string{s}};
            reusable_monotonic_buffer_resource mr;
            T y;
            ss >> as_msgpack(y);
            expect(x==y);
            ss.str({});
            ss << as_msgpack(y);
            auto r = std::move(ss).str();
            expect(that%printable_binary_cspan_t{s}==printable_binary_cspan_t{r});
            expect(that%printable_binary_cspan_t{s}==printable_binary_cspan_t{msgpack(x)});
            expect(x==msgunpack<T>({reinterpret_cast<const byte*>(s.data()),s.size()}));
            boost::asio::io_context ctx;
            boost::asio::local::stream_protocol::socket from{ctx},to{ctx};
            boost::asio::local::connect_pair(from,to);
            async_msgpack(to,x,[&](result<void> r){
                r.value();
            });
            T z;
            async_msgunpack(from,z,[&](result<void> r){
                r.value();
            });
            try{
                ctx.run();
            }catch(...){
                auto e = std::current_exception();
                expect(false);
            }
            expect(x==z);
        }
    }

    template<typename T,size_t N>
    void test_io(const T& x,const char (&s)[N])
    {
        test_io(x,{s,N-1});
    }

    suite io = []{
        test_io(nullptr,"\xc0");
        test_io(false,"\xc2");
        test_io(true,"\xc3");
        test_io(0,"\x00");
        test_io(1,"\x01");
        test_io(42,"\x2a");
        test_io(0x7e,"\x7e");
        test_io(0x7f,"\x7f");
        test_io(0x80,"\xcc\x80");
        test_io(0x81,"\xcc\x81");
        test_io(0xba,"\xcc\xba");
        test_io(0xfe,"\xcc\xfe");
        test_io(0xff,"\xcc\xff");
        test_io(0x100,"\xcd\x01\x00");
        test_io(0x101,"\xcd\x01\x01");
        test_io(0x1234,"\xcd\x12\x34");
        test_io(0xfffe,"\xcd\xff\xfe");
        test_io(0xffff,"\xcd\xff\xff");
        test_io(0x10000,"\xce\x00\x01\x00\x00");
        test_io(0x10001,"\xce\x00\x01\x00\x01");
        test_io(0x123456,"\xce\x00\x12\x34\x56");
        test_io(0xfffffffe,"\xce\xff\xff\xff\xfe");
        test_io(0xffffffff,"\xce\xff\xff\xff\xff");
        test_io(0x100000000,"\xcf\x00\x00\x00\x01\x00\x00\x00\x00");
        test_io(0x100000001,"\xcf\x00\x00\x00\x01\x00\x00\x00\x01");
        test_io(0x123456789a,"\xcf\x00\x00\x00\x12\x34\x56\x78\x9a");
        test_io(0x123456789abc,"\xcf\x00\x00\x12\x34\x56\x78\x9a\xbc");
        test_io(0x123456789abcde,"\xcf\x00\x12\x34\x56\x78\x9a\xbc\xde");
        test_io(0xfffffffffffffffe,"\xcf\xff\xff\xff\xff\xff\xff\xff\xfe");
        test_io(0xffffffffffffffff,"\xcf\xff\xff\xff\xff\xff\xff\xff\xff");
        test_io(-1,"\xff");
        test_io(-2,"\xfe");
        test_io(-7,"\xf9");
        test_io(-0x1f,"\xe1");
        test_io(-0x20,"\xe0");
        test_io(-0x21,"\xd0\xdf");
        test_io(-0x4d,"\xd0\xb3");
        test_io(-0x7f,"\xd0\x81");
        test_io(-0x80,"\xd0\x80");
        test_io(-0x81,"\xd1\xff\x7f");
        test_io(-0x82,"\xd1\xff\x7e");
        test_io(-0xee,"\xd1\xff\x12");
        test_io(-0x1234,"\xd1\xed\xcc");
        test_io(-0x7fff,"\xd1\x80\x01");
        test_io(-int32_t(0x8000),"\xd1\x80\x00");
        test_io(-int32_t(0x8001),"\xd2\xff\xff\x7f\xff");
        test_io(-int32_t(0x8002),"\xd2\xff\xff\x7f\xfe");
        test_io(-int32_t(0x9999),"\xd2\xff\xff\x66\x67");
        test_io(-int32_t(0x123456),"\xd2\xff\xed\xcb\xaa");
        test_io(-int32_t(0x7fffffff),"\xd2\x80\x00\x00\x01");
        test_io(-int64_t(0x80000000),"\xd2\x80\x00\x00\x00");
        test_io(-int64_t(0x80000001),"\xd3\xff\xff\xff\xff\x7f\xff\xff\xff");
        test_io(-int64_t(0x80000002),"\xd3\xff\xff\xff\xff\x7f\xff\xff\xfe");
        test_io(-int64_t(0x9abcdef0),"\xd3\xff\xff\xff\xff\x65\x43\x21\x10");
        test_io(-int64_t(0x789abcdef0),"\xd3\xff\xff\xff\x87\x65\x43\x21\x10");
        test_io(-int64_t(0x56789abcdef0),"\xd3\xff\xff\xa9\x87\x65\x43\x21\x10");
        test_io(-int64_t(0x3456789abcdef0),"\xd3\xff\xcb\xa9\x87\x65\x43\x21\x10");
        test_io(-int64_t(0x123456789abcdef0),"\xd3\xed\xcb\xa9\x87\x65\x43\x21\x10");
        test_io(-int64_t(0x7fffffffffffffff),"\xd3\x80\x00\x00\x00\x00\x00\x00\x01");
        test_io(-int64_t(0x7fffffffffffffff)-1,"\xd3\x80\x00\x00\x00\x00\x00\x00\x00");
        test_io(1.25f,"\xca\x3f\xa0\x00\x00");
        test_io(1.25,"\xcb\x3f\xf4\x00\x00\x00\x00\x00\x00");
        test_io(string{},"\xa0");
        test_io(string{"\x01"},"\xa1\x01");
        test_io(string{"\x01\x02"},"\xa2\x01\x02");
        test_io(event{string_header{0x1e}},"\xbe");
        test_io(event{string_header{0x1f}},"\xbf");
        test_io(event{string_header{0x20}},"\xd9\x20");
        test_io(event{string_header{0x21}},"\xd9\x21");
        test_io(event{string_header{0x7c}},"\xd9\x7c");
        test_io(event{string_header{0xfe}},"\xd9\xfe");
        test_io(event{string_header{0xff}},"\xd9\xff");
        test_io(event{string_header{0x100}},"\xda\x01\x00");
        test_io(event{string_header{0x101}},"\xda\x01\x01");
        test_io(event{string_header{0x123}},"\xda\x01\x23");
        test_io(event{string_header{0xfffe}},"\xda\xff\xfe");
        test_io(event{string_header{0xffff}},"\xda\xff\xff");
        test_io(event{string_header{0x10000}},"\xdb\x00\x01\x00\x00");
        test_io(event{string_header{0x10001}},"\xdb\x00\x01\x00\x01");
        test_io(event{string_header{0x123456}},"\xdb\x00\x12\x34\x56");
        test_io(event{string_header{0x12345678}},"\xdb\x12\x34\x56\x78");
        test_io(event{string_header{0xffffffff}},"\xdb\xff\xff\xff\xff");
        test_io(vector<byte>{},"\xc4\x00");
        test_io(vector<byte>{1,byte{0xbb}},"\xc4\x01\xbb");
        test_io(vector<byte>{2,byte{0xbb}},"\xc4\x02\xbb\xbb");
        test_io(event{binary_header{0x2a}},"\xc4\x2a");
        test_io(event{binary_header{0xfe}},"\xc4\xfe");
        test_io(event{binary_header{0xff}},"\xc4\xff");
        test_io(event{binary_header{0x100}},"\xc5\x01\x00");
        test_io(event{binary_header{0x101}},"\xc5\x01\x01");
        test_io(event{binary_header{0x4243}},"\xc5\x42\x43");
        test_io(event{binary_header{0xfffe}},"\xc5\xff\xfe");
        test_io(event{binary_header{0xffff}},"\xc5\xff\xff");
        test_io(event{binary_header{0x10000}},"\xc6\x00\x01\x00\x00");
        test_io(event{binary_header{0x10001}},"\xc6\x00\x01\x00\x01");
        test_io(event{binary_header{0x123456}},"\xc6\x00\x12\x34\x56");
        test_io(event{binary_header{0x12345678}},"\xc6\x12\x34\x56\x78");
        test_io(event{binary_header{0xffffffff}},"\xc6\xff\xff\xff\xff");
        test_io(vector<int>{},"\x90");
        test_io(vector<int>(1,0x55),"\x91\x55");
        test_io(vector<int>(2,0x55),"\x92\x55\x55");
        test_io(event{sequence_header{0xe}},"\x9e");
        test_io(event{sequence_header{0xf}},"\x9f");
        test_io(event{sequence_header{0x10}},"\xdc\x00\x10");
        test_io(event{sequence_header{0x11}},"\xdc\x00\x11");
        test_io(event{sequence_header{0x123}},"\xdc\x01\x23");
        test_io(event{sequence_header{0xfffe}},"\xdc\xff\xfe");
        test_io(event{sequence_header{0xffff}},"\xdc\xff\xff");
        test_io(event{sequence_header{0x10000}},"\xdd\x00\x01\x00\x00");
        test_io(event{sequence_header{0x10001}},"\xdd\x00\x01\x00\x01");
        test_io(event{sequence_header{0x123456}},"\xdd\x00\x12\x34\x56");
        test_io(event{sequence_header{0x12345678}},"\xdd\x12\x34\x56\x78");
        test_io(event{sequence_header{0xffffffff}},"\xdd\xff\xff\xff\xff");
        test_io(std::map<int,int>{},"\x80");
        test_io(std::map<int,int>{{1,2}},"\x81\x01\x02");
        test_io(std::map<int,int>{{1,2},{3,4}},"\x82\x01\x02\x03\x04");
        test_io(event{map_header{0xe}},"\x8e");
        test_io(event{map_header{0xf}},"\x8f");
        test_io(event{map_header{0x10}},"\xde\x00\x10");
        test_io(event{map_header{0x11}},"\xde\x00\x11");
        test_io(event{map_header{0x1234}},"\xde\x12\x34");
        test_io(event{map_header{0xfffe}},"\xde\xff\xfe");
        test_io(event{map_header{0xffff}},"\xde\xff\xff");
        test_io(event{map_header{0x10000}},"\xdf\x00\x01\x00\x00");
        test_io(event{map_header{0x10001}},"\xdf\x00\x01\x00\x01");
        test_io(event{map_header{0x456789}},"\xdf\x00\x45\x67\x89");
        test_io(event{map_header{0x456789ab}},"\xdf\x45\x67\x89\xab");
        test_io(event{map_header{0xffffffff}},"\xdf\xff\xff\xff\xff");
        test_io(event{extension_header{0x0,0x44}},"\xc7\x00\x44");
        test_io(event{extension_header{0x1,0x44}},"\xd4\x44");
        test_io(event{extension_header{0x2,0x44}},"\xd5\x44");
        test_io(event{extension_header{0x3,0x44}},"\xc7\x03\x44");
        test_io(event{extension_header{0x4,0x44}},"\xd6\x44");
        test_io(event{extension_header{0x5,0x44}},"\xc7\x05\x44");
        test_io(event{extension_header{0x8,0x44}},"\xd7\x44");
        test_io(event{extension_header{0xe,0x44}},"\xc7\x0e\x44");
        test_io(event{extension_header{0x10,0x44}},"\xd8\x44");
        test_io(event{extension_header{0x2a,0x44}},"\xc7\x2a\x44");
        test_io(event{extension_header{0xfe,0x44}},"\xc7\xfe\x44");
        test_io(event{extension_header{0xff,0x44}},"\xc7\xff\x44");
        test_io(event{extension_header{0x100,0x44}},"\xc8\x01\x00\x44");
        test_io(event{extension_header{0x101,0x44}},"\xc8\x01\x01\x44");
        test_io(event{extension_header{0xabcd,0x44}},"\xc8\xab\xcd\x44");
        test_io(event{extension_header{0xfffe,0x44}},"\xc8\xff\xfe\x44");
        test_io(event{extension_header{0xffff,0x44}},"\xc8\xff\xff\x44");
        test_io(event{extension_header{0x10000,0x44}},"\xc9\x00\x01\x00\x00\x44");
        test_io(event{extension_header{0x10001,0x44}},"\xc9\x00\x01\x00\x01\x44");
        test_io(event{extension_header{0x234567,0x44}},"\xc9\x00\x23\x45\x67\x44");
        test_io(event{extension_header{0x23456789,0x44}},"\xc9\x23\x45\x67\x89\x44");
        test_io(event{extension_header{0xffffffff,0x44}},"\xc9\xff\xff\xff\xff\x44");
        using namespace std::chrono_literals;
        test_io(timestamp_t{std::chrono::sys_days{1970y/01/01}},"\xd6\xff\x00\x00\x00\x00");
        test_io(timestamp_t{std::chrono::sys_days{2021y/03/22}+16h+43min+28s},
                "\xd6\xff\x60\x58\xc9\x30");
        test_io(timestamp_t{std::chrono::sys_days{2106y/02/07}+6h+28min+15s},
                "\xd6\xff\xff\xff\xff\xff");
        test_io(timestamp_t{std::chrono::sys_days{2106y/02/07}+6h+28min+16s},
                "\xd7\xff\x00\x00\x00\x01\x00\x00\x00\x00");
        // MessagePack 64-bit timestamp stores 34-bit unsigned seconds apart from
        // 30-bit nanoseconds, which yields a maximum of 2514-05-30 01:53:03.999999999
        // A maximum in signed 64-bit nanoseconds is lower at 2262-04-11 23:47:16.854775807.
        // Because of this we only use timestamp-96 for times before UNIX epoch (negatives).
        test_io(timestamp_t{std::chrono::sys_days{2262y/04/11}+23h+47min+16s+854775807ns},
                "\xd7\xff\xcb\xcb\x5f\xfe\x25\xc1\x7d\x04");
        test_io(timestamp_t{std::chrono::sys_days{2021y/03/22}+16h+43min+28s}+123456789ns,
                "\xd7\xff\x1d\x6f\x34\x54\x60\x58\xc9\x30");
        test_io(timestamp_t{std::chrono::sys_days{1969y/12/31}+23h+59min+59s},
                "\xc7\x0c\xff\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff");
        test_io(timestamp_t{std::chrono::sys_days{1969y/12/31}+23h+59min+59s}+999999999ns,
                "\xc7\x0c\xff\x3b\x9a\xc9\xff\xff\xff\xff\xff\xff\xff\xff\xff");
        test_io(timestamp_t{std::chrono::sys_days{1813y/03/11}+5h+3min+59s},
                "\xc7\x0c\xff\x00\x00\x00\x00\xff\xff\xff\xfe\xd9\x0c\x90\x3f");
        // Due to the same limitation this is as low as we can go.
        test_io(timestamp_t{std::chrono::sys_days{1677y/9/21}+0h+12min+44s}+854775808ns,
                "\xc7\x0c\xff\x32\xf2\xd8\x00\xff\xff\xff\xfd\xda\x3e\x82\xfc");
        test_io(value{{{
            {"abc",true},
            {"defg",-3},
            {"ghi",{{12,-2,true}}},
            {"test",nullptr}
        }}},"\x84\xa3""abc""\xc3\xa4""defg""\xfd\xa3""ghi""\x93\x0c\xfe\xc3\xa4""test""\xc0");
    };
}}
