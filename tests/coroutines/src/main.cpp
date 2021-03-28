// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/tests/ut_helpers.hpp>

#include <ampi/coro/use_coroutine.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/static_thread_pool.hpp>
#include <boost/asio/steady_timer.hpp>

#include <thread>

namespace ampi { namespace
{
    using namespace boost::ut;

    const std::thread::id main_tid = std::this_thread::get_id();

    suite coroutines = []{
        "lazy_function"_test = []{
            auto lv = []() -> noexcept_lazy_function<int> {
                co_return 42;
            };
            "destroy without execution"_test = [&]{
                [[maybe_unused]] auto lvi = lv();
            };
            "execute"_test = [&]{
                expect(lv()()==42_i);
            };
            "void return"_test = []{
                bool did_run = false;
                [](bool& did_run) -> noexcept_lazy_function<> {
                    did_run = true;
                    co_return;
                }(did_run)();
                expect(did_run);
            };
            "nontrivial_return"_test = []{
                std::string s = []() -> lazy_function<std::string> {
                    co_return "zerg";
                }()();
                expect(s=="zerg");
            };
            "throwing"_test = []{
                auto elv_v = []() -> lazy_function<> {
                    throw std::runtime_error{"test error"};
                    co_return;
                }();
                bool thrown = false;
                try{
                    std::move(elv_v)();
                }
                catch(const std::runtime_error&){
                    thrown = true;
                }
                expect(thrown);
            };
        };
        "generator"_test = []{
            "empty generator"_test = []{
                auto empty_gen = []() -> noexcept_generator<int> {
                    co_return;
                };
                {
                    auto empty_gen_v = empty_gen();
                    int* ret = empty_gen_v();
                    expect(ret==nullptr_v);
                }
                {
                    auto empty_gen_v = empty_gen();
                    expect(empty_gen_v.begin()==empty_gen_v.end());
                }
            };
            "non-empty generator"_test = []{
                auto gen = []() -> noexcept_generator<int> {
                    co_yield 1;
                    co_yield 2;
                };
                std::array res = {1,2};
                {
                    auto gen_v = gen();
                    for(int i:res){
                        int* ret = gen_v();
                        expect(ret!=nullptr_v);
                        if(ret)
                            expect(that % *ret==i);
                    }
                    expect(gen_v()==nullptr_v);
                }
                {
                    auto gen_v = gen();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
            };
            "throwing generator"_test = []{
                auto gen_v = []() -> generator<int> {
                    co_yield 1;
                    throw std::runtime_error{"test error"};
                }();
                int* res = gen_v();
                expect(res!=nullptr_v);
                if(res){
                    expect(*res==1_i);
                    bool thrown = false;
                    try{
                        gen_v();
                    }
                    catch(const std::runtime_error&){
                        thrown = true;
                    }
                    expect(thrown);
                }
            };
            "nested lazy_function"_test = []{
                auto gen_v = []() -> noexcept_generator<int> {
                    expect([]() -> noexcept_lazy_function<int> {
                        co_return 42;
                    }()()==42_i);
                    co_return;
                }();
                expect(gen_v()==nullptr_v);
            };
            "delegated yield"_test = []{
                {
                    std::array<int,0> res;
                    auto gen_v = []() -> noexcept_delegating_generator<int> {
                        co_return;
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1};
                    auto gen_v = []() -> noexcept_delegating_generator<int> {
                        co_yield 1;
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array<int,0> res;
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield []() -> noexcept_generator<int> {
                            co_return;
                        }();
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2,3};
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield 1;
                        co_yield []() -> noexcept_generator<int> {
                            co_yield 2;
                        }();
                        co_yield 3;
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2,3};
                    auto gen_v = []() -> noexcept_generator<int> {
                            co_yield 2;
                    }();
                    auto gen_v2 = [](auto& gen) -> noexcept_delegating_generator<int> {
                        co_yield 1;
                        co_yield std::move(gen);
                        co_yield 3;
                    }(gen_v);
                    expect(std::equal(gen_v2.begin(),gen_v2.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2,3,4,5};
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield 1;
                        co_yield []() -> delegating_generator<int> {
                            co_yield 2;
                            co_yield []() -> noexcept_generator<int> {
                                co_yield 3;
                            }();
                            co_yield 4;
                        }();
                        co_yield 5;
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2,3,4,5,6,7};
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield 1;
                        co_yield []() -> delegating_generator<int> {
                            co_yield 2;
                            co_yield []() -> delegating_generator<int> {
                                co_yield 3;
                                co_yield []() -> noexcept_generator<int> {
                                    co_yield 4;
                                }();
                                co_yield 5;
                            }();
                            co_yield 6;
                        }();
                        co_yield 7;
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array<int,0> res;
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield tail_yield_to([]() -> noexcept_generator<int> {
                            co_return;
                        }());
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2};
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield 1;
                        co_yield tail_yield_to([]() -> noexcept_generator<int> {
                            co_yield 2;
                        }());
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2,3,4,5,6,7};
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield 1;
                        co_yield []() -> delegating_generator<int> {
                            co_yield 2;
                            co_yield tail_yield_to([]() -> delegating_generator<int> {
                                co_yield tail_yield_to([]() -> delegating_generator<int> {
                                    co_yield 3;
                                    co_yield []() -> delegating_generator<int> {
                                        co_yield 4;
                                        co_yield tail_yield_to([]() -> noexcept_delegating_generator<int> {
                                            co_yield 5;
                                        }());
                                    }();
                                    co_yield 6;
                                }());
                            }());
                        }();
                        co_yield 7;
                    }();
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                }
                {
                    std::array res{1,2};
                    auto gen_v = []() -> delegating_generator<int> {
                        co_yield 1;
                        co_yield []() -> generator<int> {
                            co_yield 2;
                            throw std::runtime_error{"test"};
                        }();
                        co_yield 3;
                    }();
                    std::exception_ptr e;
                    std::vector<int> v;
                    try{
                        while(auto p = gen_v())
                            v.push_back(*p);
                    }
                    catch(const std::runtime_error&){
                        e = std::current_exception();
                    }
                    expect(std::equal(v.begin(),v.end(),res.begin(),res.end()));
                    expect(!!e);
                }
            };
        };
        "coroutine"_test = []{
            "system executor"_test = []{
                []() -> noexcept_coroutine<int> {
                    co_return 42;
                }().async_run([=](int res){
                    expect(main_tid==std::this_thread::get_id());
                    expect(res==42_i);
                });
            };
            "executor that doesn't change type with work tracking"_test = []{
                boost::asio::io_context ctx;
                [](boost::asio::any_io_executor)
                        -> noexcept_coroutine<void,boost::asio::any_io_executor> {
                    co_return;
                }(ctx.get_executor()).async_run([]{});
                ctx.run();
            };
            "thread pool executor"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                [](executor_type ex)
                        -> noexcept_coroutine<int,executor_type> {
                    expect(ex.running_in_this_thread());
                    co_return 42;
                }(tp1.get_executor()).async_run(boost::asio::bind_executor(tp2.get_executor(),
                        [&](int res){
                    expect(tp2.get_executor().running_in_this_thread());
                    expect(res==42_i);
                }));
                tp1.join();
                tp2.join();
            };
            "multiple executor hops and throwing"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1},tp3{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                using timer_t = typename boost::asio::steady_timer::
                    rebind_executor<executor_type>::other;
                auto timer = use_coroutine_t::as_default_on(timer_t{tp3.get_executor()});
                auto c2 = [](executor_type ex,auto& timer)
                        -> coroutine<int,executor_type> {
                    expect(ex.running_in_this_thread());
                    timer.expires_after(boost::asio::chrono::milliseconds{10});
                    co_await timer.async_wait();
                    expect(ex.running_in_this_thread());
                    throw std::runtime_error{"test error"};
                    co_return 17;
                }(tp2.get_executor(),timer);
                [](executor_type ex,auto& c2)
                        -> noexcept_coroutine<int,executor_type> {
                    expect(ex.running_in_this_thread());
                    int x = 0;
                    try{
                        x = co_await std::move(c2);
                    }
                    catch(const std::runtime_error& e){
                        x = 42;
                    }
                    expect(ex.running_in_this_thread());
                    expect(x==42_i);
                    co_return x;
                }(tp1.get_executor(),c2).async_run(boost::asio::bind_executor(tp3.get_executor(),
                        [&](int res){
                    expect(tp3.get_executor().running_in_this_thread());
                    expect(res==42);
                }));
                tp1.join();
                tp2.join();
                tp3.join();
            };
            "nested lazy_function/generator"_test = []{
                []() -> subcoroutine<> {
                    expect([]() -> noexcept_lazy_function<int> {
                        co_return 42;
                    }()()==42_i);
                    auto gen_v = []() -> noexcept_generator<int> {
                        co_yield 1;
                        co_yield 2;
                    }();
                    std::array res = {1,2};
                    expect(std::equal(gen_v.begin(),gen_v.end(),res.begin(),res.end()));
                    co_return;
                }().assume_blocking()();
            };
            "subroutine"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                [](executor_type ex1,executor_type ex2)
                        -> noexcept_coroutine<void,executor_type> {
                    expect(ex1.running_in_this_thread());
                    auto sub = [](executor_type ex1,executor_type ex2)
                            -> noexcept_coroutine<void,executor_type>{
                        expect(ex1.running_in_this_thread());
                        co_await [](executor_type ex2)
                                -> noexcept_subcoroutine<void,executor_type> {
                            expect(ex2.running_in_this_thread());
                            co_return;
                        }(ex2);
                        expect(ex1.running_in_this_thread());
                    };
                    co_await sub(ex1,ex2);
                    expect(ex1.running_in_this_thread());
                    co_await sub(ex1,ex2);
                    expect(ex1.running_in_this_thread());
                }(tp1.get_executor(),tp2.get_executor())();
                tp1.join();
                tp2.join();
            };
            "sync run on non-system executor"_test = []{
                boost::asio::static_thread_pool tp{1};
                auto coro_v = []() -> subcoroutine<int> {
                    throw std::runtime_error{"error"};
                    co_return 0;
                }().assume_blocking();
                bool thrown = false;
                try{
                    [[maybe_unused]] int x = std::move(coro_v)();
                }
                catch(const std::runtime_error&){
                    thrown = true;
                }
                expect(thrown);
                tp.join();
            };
            "executor changing"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                [](executor_type ex1,executor_type ex2)
                        -> noexcept_coroutine<void,boost::asio::static_thread_pool::executor_type> {
                    expect(ex1.running_in_this_thread());
                    co_await ex2;
                    expect(ex2.running_in_this_thread());
                }(tp1.get_executor(),tp2.get_executor())();
                tp1.join();
                tp2.join();
            };
        };
        "async_generator"_test = []{
            "ping-pong"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1},tp3{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                auto c2 = [](executor_type ex2)
                        -> noexcept_async_generator<int,executor_type> {
                    expect(ex2.running_in_this_thread());
                    co_yield 1;
                    expect(ex2.running_in_this_thread());
                    co_yield 2;
                    expect(ex2.running_in_this_thread());
                    co_yield 3;
                    expect(ex2.running_in_this_thread());
                }(tp2.get_executor());
                [](executor_type ex,auto& c2)
                        -> noexcept_coroutine<int,executor_type> {
                    expect(ex.running_in_this_thread());
                    int res = 0;
                    while(auto p = co_await c2){
                        expect(ex.running_in_this_thread());
                        res += *p;
                    }
                    expect(res==6_i);
                    co_return std::move(res);
                }(tp1.get_executor(),c2).async_run(boost::asio::bind_executor(tp3.get_executor(),
                        [&](int res){
                    expect(tp3.get_executor().running_in_this_thread());
                    expect(res==6_i);
                }));
                tp1.join();
                tp2.join();
                tp3.join();
            };
            "nesting and throwing"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1},tp3{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                auto gen_v = [](executor_type,executor_type other_executor)
                        -> async_generator<int> {
                    auto coro_v = [](executor_type) -> coroutine<int> {
                        throw std::runtime_error{"first error"};
                        co_return 0;
                    }(other_executor);
                    bool thrown = false;
                    try{
                        co_await std::move(coro_v);
                    }
                    catch(const std::runtime_error&){
                        thrown = true;
                    }
                    expect(thrown);
                    auto sync_gen_v = []() -> generator<int> {
                        throw std::runtime_error{"second error"};
                    }();
                    co_yield std::move(*co_await sync_gen_v);
                }(tp1.get_executor(),tp2.get_executor());
                gen_v.async_run(boost::asio::bind_executor(tp3.get_executor(),[=]
                        (decltype(gen_v)::run_result_type res){
                    bool required_thrown = false;
                    try{
                        [[maybe_unused]] int* ret = res.value();
                    }
                    catch(const std::runtime_error& e){
                        required_thrown = e.what()==std::string_view{"second error"};
                    }
                    expect(required_thrown);
                }));
                tp1.join();
                tp2.join();
                tp3.join();
            };
            "executor changing"_test = []{
                boost::asio::static_thread_pool tp1{1},tp2{1};
                using executor_type = boost::asio::static_thread_pool::executor_type;
                auto gen_v = [](executor_type ex1,executor_type ex2)
                        -> noexcept_async_generator<int,executor_type> {
                    expect(ex1.running_in_this_thread());
                    co_yield 1;
                    expect(co_await this_coroutine::executor==ex2);
                    expect(ex2.running_in_this_thread());
                }(tp1.get_executor(),tp2.get_executor());
                [[maybe_unused]] int* res = gen_v();
                gen_v.set_executor(tp2.get_executor());
                res = gen_v();
                tp1.join();
                tp2.join();
            };
        };
    };
}}
