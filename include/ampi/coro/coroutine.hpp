// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_E15F4C61_C7C9_468F_B22C_A1702FB119C8
#define UUID_E15F4C61_C7C9_468F_B22C_A1702FB119C8

#include <ampi/coro/awaiter_wrapper.hpp>
#include <ampi/coro/coro_handle_owner.hpp>
#include <ampi/execution/executor.hpp>
#include <ampi/execution/prefer.hpp>
#include <ampi/execution/query.hpp>
#include <ampi/execution/traits.hpp>
#include <ampi/utils/bit.hpp>
#include <ampi/utils/empty_subobject.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/execution/execute.hpp>
#include <boost/asio/execution/executor.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/outcome/boost_result.hpp>
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <algorithm>
#include <cassert>
#include <exception>
#include <iterator>
#include <latch>
#include <memory>
#include <new>
#include <optional>
#include <tuple>
#include <utility>

namespace ampi
{
    enum coroutine_option : uint8_t
    {
        none              = 0,
        handle_exceptions = 0b0000001,
        save_awaiter      = 0b0000010,
        subservant        = 0b0000100,
        assume_blocking   = 0b0001000,
        support_yield     = 0b0010000,
        delegated_yield   = 0b0100000,

        orphan_           = 0b1000000
    };

    // TODO: use flags<coroutine_option> and scoped coroutine_option
    //       when CTNTTP is supported everywhere.
    using coroutine_options = uint8_t;

    template<coroutine_options Options,typename Result,executor Executor>
    class basic_coroutine;

    template<typename Result = void,executor Executor = boost::asio::system_executor>
    using lazy_function = basic_coroutine<coroutine_option::handle_exceptions,
                                          Result,Executor>;

    template<typename Result = void,executor Executor = boost::asio::system_executor>
    using noexcept_lazy_function = basic_coroutine<coroutine_option::none,
                                                   Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using generator = basic_coroutine<coroutine_option::handle_exceptions|
                                      coroutine_option::support_yield,
                                      Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using noexcept_generator = basic_coroutine<coroutine_option::support_yield,
                                               Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using delegating_generator = basic_coroutine<coroutine_option::handle_exceptions|
                                                 coroutine_option::support_yield|
                                                 coroutine_option::delegated_yield,
                                                 Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using noexcept_delegating_generator = basic_coroutine<coroutine_option::support_yield|
                                                          coroutine_option::delegated_yield,
                                                          Result,Executor>;

    template<typename Result = void,executor Executor = boost::asio::system_executor>
    using coroutine = basic_coroutine<coroutine_option::handle_exceptions|
                                      coroutine_option::save_awaiter,
                                      Result,Executor>;

    template<typename Result = void,executor Executor = boost::asio::system_executor>
    using subcoroutine = basic_coroutine<coroutine_option::handle_exceptions|
                                         coroutine_option::save_awaiter|
                                         coroutine_option::subservant,
                                         Result,Executor>;

    template<typename Result = void,executor Executor = boost::asio::system_executor>
    using noexcept_coroutine = basic_coroutine<coroutine_option::save_awaiter,
                                               Result,Executor>;

    template<typename Result = void,executor Executor = boost::asio::system_executor>
    using noexcept_subcoroutine = basic_coroutine<coroutine_option::save_awaiter|
                                                  coroutine_option::subservant,
                                                  Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using async_generator = basic_coroutine<coroutine_option::handle_exceptions|
                                            coroutine_option::save_awaiter|
                                            coroutine_option::support_yield,
                                            Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using async_subgenerator = basic_coroutine<coroutine_option::handle_exceptions|
                                               coroutine_option::save_awaiter|
                                               coroutine_option::subservant|
                                               coroutine_option::support_yield,
                                               Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using noexcept_async_generator = basic_coroutine<coroutine_option::save_awaiter|
                                                     coroutine_option::support_yield,
                                                     Result,Executor>;

    template<typename Result,executor Executor = boost::asio::system_executor>
    using noexcept_async_subgenerator = basic_coroutine<coroutine_option::save_awaiter|
                                                        coroutine_option::subservant|
                                                        coroutine_option::support_yield,
                                                        Result,Executor>;

    namespace detail
    {
        template<typename T,typename Result,bool ExactResult,bool Yield>
        struct basic_coroutine_concept_impl : std::false_type {};

        template<coroutine_options Options,typename ActualResult,executor Executor,
                 typename Result,bool ExactResult,bool Yield>
        struct basic_coroutine_concept_impl<basic_coroutine<Options,ActualResult,Executor>,
                                            Result,ExactResult,Yield>
            : std::bool_constant<(ExactResult?std::is_same_v<ActualResult,Result>
                                             :std::is_convertible_v<ActualResult,Result>)&&
                                  (!Yield==!(Options&coroutine_option::support_yield))> {};
    }

    template<typename T,typename Result>
    concept coroutine_returning = detail::basic_coroutine_concept_impl<
        T,Result,false,false>::value;

    template<typename T,typename Result>
    concept noexcept_coroutine_returning = coroutine_returning<T,Result>&&
        (!(T::options&coroutine_option::handle_exceptions));

    template<typename T,typename Result>
    concept lazy_function_returning = coroutine_returning<T,Result>&&
        (!(T::options&coroutine_option::save_awaiter));

    template<typename T,typename Result>
    concept noexcept_lazy_function_returning = lazy_function_returning<T,Result>&&
        (!(T::options&coroutine_option::handle_exceptions));

    template<typename T,typename Result>
    concept async_generator_yielding = detail::basic_coroutine_concept_impl<
        T,Result,false,true>::value;

    template<typename T,typename Result>
    concept noexcept_async_generator_yielding = async_generator_yielding<T,Result>&&
        (!(T::options&coroutine_option::handle_exceptions));

    template<typename T,typename Result>
    concept async_generator_yielding_exactly = detail::basic_coroutine_concept_impl<
        T,Result,true,true>::value;

    template<typename T,typename Result>
    concept noexcept_async_generator_yielding_exactly = async_generator_yielding_exactly<T,Result>&&
        (!(T::options&coroutine_option::handle_exceptions));

    template<typename T,typename Result>
    concept generator_yielding = async_generator_yielding<T,Result>&&
        (!(T::options&coroutine_option::save_awaiter));

    template<typename T,typename Result>
    concept generator_yielding_exactly = async_generator_yielding_exactly<T,Result>&&
        (!(T::options&coroutine_option::save_awaiter));

    template<typename T,typename Result>
    concept noexcept_generator_yielding = generator_yielding<T,Result>&&
        (!(T::options&coroutine_option::handle_exceptions));

    template<typename T,typename Result>
    concept noexcept_generator_yielding_exactly = generator_yielding_exactly<T,Result>&&
        (!(T::options&coroutine_option::handle_exceptions));
        
    namespace this_coroutine
    {
        constexpr inline struct executor_t {} executor;
    }

    namespace detail
    {
        template<typename Generator>
        struct tail_yield_to_t
        {
            Generator&& subgen_;
        };
    }

    template<typename T>
        requires (!std::is_reference_v<T>)&&
                 async_generator_yielding_exactly<T,std::remove_pointer_t<
                    typename T::result_type>>
    auto tail_yield_to(T&& subgen)
    {
        return detail::tail_yield_to_t<T>{std::move(subgen)};
    }

    namespace detail
    {
        struct coroutine_promise_initial
        {
            stdcoro::suspend_always initial_suspend() const noexcept
            {
                return {};
            }
        };

        template<coroutine_options ExceptionHandling /* & handle_exceptions = 0 */>
        struct coroutine_promise_exceptions
        {
            [[noreturn]] void unhandled_exception()
            {
                BOOST_UNREACHABLE_RETURN(void());
            }

            void rethrow_exception() noexcept {}
        };

        template<>
        struct coroutine_promise_exceptions<coroutine_option::handle_exceptions>
        {
            [[noreturn]] void unhandled_exception()
            {
                throw;
            }

            void rethrow_exception() noexcept {}
        };

        template<>
        struct coroutine_promise_exceptions<coroutine_option::handle_exceptions|
                                            coroutine_option::save_awaiter>
        {
            std::exception_ptr e_;

            void unhandled_exception() noexcept
            {
                e_ = std::current_exception();
            }

            void rethrow_exception()
            {
                if(e_)
                    std::rethrow_exception(std::move(e_));
            }
        };

        template<coroutine_options AwaiterHandling /* = none*/>
        struct coroutine_promise_awaiter {};

        template<>
        struct coroutine_promise_awaiter<coroutine_option::save_awaiter>
        {
            coro_handle_owner<> awaiter_;

            auto switch_to_awaiter_awaitable() noexcept
            {
                struct awaitable : stdcoro::suspend_always
                {
                    coro_handle_owner<>& awaiter_;

                    stdcoro::coroutine_handle<> await_suspend(stdcoro::coroutine_handle<> /*handle*/) noexcept
                    {
                        return std::move(awaiter_).release();
                    }
                };
                return awaitable{{},awaiter_};
            }
        };

        template<coroutine_options Options /* &coroutine_option::support_yield==0*/,typename Result>
        struct coroutine_promise_result :
            coroutine_promise_awaiter<Options&~coroutine_option::handle_exceptions>
        {
            std::conditional_t<std::is_trivially_default_constructible_v<Result>,
                               Result,std::optional<Result>> result_;

            void return_value(Result result) noexcept
            {
                result_ = std::move(result);
            }

            Result result() noexcept
            {
                if constexpr(std::is_trivially_default_constructible_v<Result>)
                    return std::move(result_);
                else
                    return std::move(*result_);
            }
        };

        template<coroutine_options Options>
            requires (!(Options&coroutine_option::support_yield))
        struct coroutine_promise_result<Options,void>
            : coroutine_promise_awaiter<Options&~coroutine_option::handle_exceptions>
        {
            void return_void() noexcept {}

            void result() const noexcept {}
        };

        template<coroutine_options Options,typename Result>
            requires (bool(Options&coroutine_option::support_yield))&&
                     (bool(Options&coroutine_option::handle_exceptions))&&
                     (!(Options&coroutine_option::delegated_yield))
        struct coroutine_promise_result<Options,Result> :
            coroutine_promise_awaiter<Options&~(coroutine_option::handle_exceptions|
                                                coroutine_option::support_yield)>
        {
            static_assert(!std::is_void_v<Result>,"async_generator cannot produce void");

            // Unlike with return_value, a yielded value bound to a reference
            // parameter is kept alive while the coroutine is suspended, so
            // we don't need an optional to store it.

            Result* result_;

            void return_void() noexcept
            {
                result_ = {};
            }

            auto yield_value(Result&& result) noexcept
            {
                result_ = std::addressof(result);
                if constexpr(bool(Options&coroutine_option::save_awaiter))
                    return this->switch_to_awaiter_awaitable();
                else
                    return stdcoro::suspend_always{};
            }

            Result* result() noexcept
            {
                return result_;
            }
        };

        template<coroutine_options Options,typename Result>
            requires (bool(Options&coroutine_option::support_yield))&&
                     (!(Options&coroutine_option::handle_exceptions))&&
                     (!(Options&coroutine_option::delegated_yield))
        struct coroutine_promise_result<Options,Result> :
            coroutine_promise_result<Options|coroutine_option::handle_exceptions,Result> {};

        template<coroutine_options Options,typename Result>
            requires (bool(Options&coroutine_option::delegated_yield))&&
                     (bool(Options&coroutine_option::handle_exceptions))
        struct coroutine_promise_result<Options,Result> :
            coroutine_promise_result<Options&~coroutine_option::delegated_yield,Result>
        {
            using base_t = coroutine_promise_result<
                Options&~coroutine_option::delegated_yield,Result>;

            union
            {
                // In subgenerators
                coroutine_promise_result* top_;
                // In top generator
                base_t* current_;
            };
            // In subgenerators, prev_ is their predecessor,
            // in top generator, prev_ is one after current.
            coroutine_promise_result *prev_ = nullptr;

            coroutine_promise_result() noexcept
                : current_{this}
            {}

            template<typename T,bool ReplaceCurrent = false>
            auto do_yield(T&& subgen) noexcept
            {
                struct switch_to_subgen_awaitable : stdcoro::suspend_always
                {
                    stdcoro::coroutine_handle<> handle_;

                    stdcoro::coroutine_handle<> await_suspend(
                        stdcoro::coroutine_handle<>) noexcept
                    {
                        return handle_;
                    }

                    void await_resume() noexcept
                    {
                        if constexpr(ReplaceCurrent)
                            BOOST_UNREACHABLE_RETURN(void());
                    }
                };
                if constexpr(bool(T::options&coroutine_option::delegated_yield)){
                    subgen->top_ = top_;
                    subgen->prev_ = ReplaceCurrent?prev_:this;
                }
                if constexpr(!ReplaceCurrent)
                    top_->prev_ = this;
                top_->current_ = subgen.operator->();
                return switch_to_subgen_awaitable{{},subgen.get()};
            }

            using base_t::yield_value;

            template<generator_yielding_exactly<Result> T>
            auto yield_value(T&& subgen) noexcept
                requires (!std::is_reference_v<T>)
            {
                return do_yield(std::move(subgen));
            }

            template<generator_yielding_exactly<Result> T>
            auto yield_value(detail::tail_yield_to_t<T> tyt) noexcept
            {
                return do_yield<T,true>(std::move(tyt.subgen_));
            }

            Result* result() noexcept
            {
                return current_->result_;
            }
        };

        template<coroutine_options Options,typename Result>
            requires (bool(Options&coroutine_option::delegated_yield))&&
                     (!(Options&coroutine_option::handle_exceptions))
        struct coroutine_promise_result<Options,Result> :
            coroutine_promise_result<Options|coroutine_option::handle_exceptions,Result>
        {
            using coroutine_promise_result<(Options&~coroutine_option::delegated_yield)|
                coroutine_option::handle_exceptions,Result>::yield_value;

            template<noexcept_generator_yielding_exactly<Result> T>
            auto yield_value(T&& subgen) noexcept
                requires (!std::is_reference_v<T>)
            {
                return this->do_yield(std::move(subgen));
            }

            template<noexcept_generator_yielding_exactly<Result> T>
            auto yield_value(detail::tail_yield_to_t<T> tyt) noexcept
            {
                return this->template do_yield<T,true>(std::move(tyt.subgen_));
            }
        };

        template<coroutine_options Options,typename ResultBase,executor Executor>
        struct coroutine_promise_executor : ResultBase
        {
            using allocator_query_result_t = query_result_t<Executor,
                boost::asio::execution::allocator_t<void>>;
            using allocator_type = typename std::allocator_traits<
                std::conditional_t<std::is_void_v<allocator_query_result_t>,
                    std::allocator<void>,allocator_query_result_t>>::template rebind_alloc<byte>;

            // If we are subservant, we always use work_ as untracked.
            // If we are not subservant, we start from work_ as tracked.
            // When we finish, we overwrite work_, if tracked/untracked types match,
            // or reconstruct as ex_ otherwise.
            // We require that we are given untracked executors upon cosntruction and
            // in set_executor so that we can return the same from get_executor
            // consistently without introducing additional state to save original
            // tracking state in case it is not reflected in the type of executor.

            using work_t = std::remove_reference_t<prefer_result_t<Executor,
                std::conditional_t<bool(Options&coroutine_option::subservant),
                    boost::asio::execution::outstanding_work_t::untracked_t,
                    boost::asio::execution::outstanding_work_t::tracked_t>>>;

            constexpr static bool work_is_executor = std::is_same_v<work_t,Executor>;

            [[no_unique_address]] union {
                [[no_unique_address]] Executor ex_;
                [[no_unique_address]] work_t work_;
            };

            ~coroutine_promise_executor()
            {
                // Ignore non-trivial destruction of union above.
                // handle::from_promise().done() in derived class will be used
                // to know which subobject is active and needs to be destroyed.
                // This saves a field for discriminator in contrast to std::variant.
            }

            // FIXME: Destroying operator delete for promise types is not allowed
            //        by the standard, see: https://bugs.llvm.org/show_bug.cgi?id=48344,
            //        so we end up storing two copies of non-empty allocators,
            //        one for the non-destroying delete and one for the promise/coroutine.
            //        On the other hand, this allows delete overload to be shifted
            //        to base class where complete type of promise is not needed.

            template<executor OtherExecutor = Executor>
                requires std::is_convertible_v<OtherExecutor,Executor>
            void* operator new(std::size_t n,OtherExecutor ex = {},auto&&... /*args*/)
            {
                if constexpr(std::is_empty_v<allocator_type>)
                    return std::to_address(allocator_type{}.allocate(n));
                else{
                    allocator_type a{boost::asio::query(Executor{std::move(ex)},
                        boost::asio::execution::allocator)};
                    size_t allocator_offset = round_up_pot(n,alignof(allocator_type));
                    byte* ret = std::to_address(a.allocate(allocator_offset+sizeof(allocator_type)));
                    ::new (static_cast<void*>(ret+allocator_offset)) allocator_type{std::move(a)};
                    return ret;
                }
            }

            void operator delete(void* p,std::size_t n)
            {
                auto c = static_cast<byte*>(p);
                auto fp = std::pointer_traits<typename std::allocator_traits<allocator_type>::pointer>
                              ::pointer_to(*c);
                if constexpr(std::is_empty_v<allocator_type>)
                    allocator_type{}.deallocate(fp,n);
                else{
                    size_t allocator_offset = round_up_pot(n,alignof(allocator_type));
                    auto a = reinterpret_cast<allocator_type*>(c+allocator_offset);
                    allocator_type alloc{std::move(*a)};
                    std::destroy_at(a);
                    alloc.deallocate(fp,allocator_offset+sizeof(allocator_type));
                }
            }

            coroutine_promise_executor(Executor ex = {},auto&&... /*args*/) noexcept
            {
                assert(boost::asio::query(ex,boost::asio::execution::outstanding_work)==
                    boost::asio::execution::outstanding_work.untracked);
                if constexpr(bool(Options&coroutine_option::subservant))
                    ::new (static_cast<void*>(&work_)) work_t{std::move(ex)};
                else
                    ::new (static_cast<void*>(&work_)) work_t{
                        boost::asio::prefer(std::move(ex),
                            boost::asio::execution::outstanding_work.tracked)};
            }

            coroutine_promise_executor(coroutine_promise_executor&&) = delete;

            allocator_type get_work_allocator() const noexcept
            {
                if constexpr(std::is_void_v<allocator_query_result_t>)
                    return {};
                else
                    return boost::asio::query(work_,boost::asio::execution::allocator);
            }

            auto final_suspend() noexcept
            {
                if constexpr(!(Options&coroutine_option::subservant)){
                    if constexpr(coroutine_promise_executor::work_is_executor)
                        work_ = boost::asio::prefer(work_,
                            boost::asio::execution::outstanding_work.untracked);
                    else{
                        Executor ex = boost::asio::prefer(work_,
                            boost::asio::execution::outstanding_work.untracked);
                        std::destroy_at(&work_);
                        std::construct_at(&ex_,std::move(ex));
                    }
                }
                if constexpr(bool(Options&coroutine_option::save_awaiter))
                    return this->switch_to_awaiter_awaitable();
                else if constexpr(bool(Options&coroutine_option::orphan_))
                    return stdcoro::suspend_never{};
                else
                    return stdcoro::suspend_always{};
            }
        };

        template<typename Args,typename Initiation,typename... InitArgs>
        struct use_coroutine_awaitable;

        template<coroutine_options Options,typename Result,executor Executor>
        struct coroutine_promise :
            coroutine_promise_executor<Options&(coroutine_option::save_awaiter|
                                                coroutine_option::subservant|
                                                coroutine_option::orphan_),
                coroutine_promise_result<Options&(coroutine_option::handle_exceptions|
                                                  coroutine_option::support_yield|
                                                  coroutine_option::delegated_yield|
                                                  coroutine_option::save_awaiter),Result>,
                Executor>,
            coroutine_promise_exceptions<Options&(coroutine_option::handle_exceptions|
                                                  coroutine_option::save_awaiter)>,
            coroutine_promise_initial
        {
            static_assert(Options&coroutine_option::save_awaiter||
                          is_trivial_executor_v<Executor>,
                          "only trivial executors are supported when not saving awaiter");
            static_assert(Options&coroutine_option::support_yield||
                          !(Options&coroutine_option::delegated_yield),
                          "basic yield support required for delegated yield");
            static_assert(!(Options&coroutine_option::delegated_yield)||
                          !(Options&coroutine_option::save_awaiter),
                          "asynchronous delegating generators are not supported yet");

            using base_t = coroutine_promise_executor<Options&(coroutine_option::save_awaiter|
                                                               coroutine_option::subservant|
                                                               coroutine_option::orphan_),
                coroutine_promise_result<Options&(coroutine_option::handle_exceptions|
                                                  coroutine_option::support_yield|
                                                  coroutine_option::delegated_yield|
                                                  coroutine_option::save_awaiter),Result>,Executor>;
            using coroutine_t = basic_coroutine<Options,Result,Executor>;

            using typename base_t::work_t;
            using typename base_t::allocator_type;

            using base_t::base_t;

            coroutine_t get_return_object() noexcept;

            auto result() noexcept(!(Options&coroutine_option::handle_exceptions)||
                                   !(Options&coroutine_option::save_awaiter))
            {
                this->rethrow_exception();
                return base_t::result();
            }

            bool has_work() const noexcept
            {
                return !stdcoro::coroutine_handle<coroutine_promise>::from_promise(
                    const_cast<coroutine_promise&>(*this)).done();
            }

            Executor get_executor() const noexcept
            {
                if constexpr(!(Options&coroutine_option::subservant)){
                    if(has_work())
                        return boost::asio::prefer(this->work_,
                            boost::asio::execution::outstanding_work.untracked);
                    else if constexpr(!base_t::work_is_executor)
                        return this->ex_;
                    else
                        return this->work_;
                }else
                    return this->work_;
            }

            void set_executor(Executor new_executor) noexcept
            {
                if constexpr(!std::is_void_v<typename base_t::allocator_query_result_t>)
                    assert(boost::asio::query(get_executor(),boost::asio::execution::allocator)==
                           boost::asio::query(new_executor,boost::asio::execution::allocator));
                assert(boost::asio::query(new_executor,boost::asio::execution::outstanding_work)==
                    boost::asio::execution::outstanding_work.untracked);
                if constexpr(!(Options&coroutine_option::subservant)){
                    if(has_work())
                        this->work_ = boost::asio::prefer(new_executor,
                            boost::asio::execution::outstanding_work.tracked);
                    else if constexpr(!base_t::work_is_executor)
                        this->ex_ = new_executor;
                    else
                        this->work_ = new_executor;
                }else
                    this->work_ = new_executor;
            }

            ~coroutine_promise()
            {
                if((Options&coroutine_option::subservant)||(!base_t::work_is_executor&&has_work()))
                    std::destroy_at(&this->work_);
                else
                    std::destroy_at(&this->ex_);
            }

            auto await_transform(this_coroutine::executor_t) noexcept
            {
                struct executor_awaiter : stdcoro::suspend_never
                {
                    coroutine_promise* promise_;

                    Executor await_resume() noexcept
                    {
                        return promise_->get_executor();
                    }
                };
                return executor_awaiter{{},this};
            }

            auto await_transform(Executor new_executor) noexcept
            {
                if constexpr(is_trivial_executor_v<Executor>)
                    return stdcoro::suspend_never{};
                else{
                    assert(boost::asio::query(this->work_,boost::asio::execution::allocator)==
                        boost::asio::query(new_executor,boost::asio::execution::allocator));
                    this->work_ = boost::asio::prefer(std::move(new_executor),
                        boost::asio::execution::outstanding_work.tracked);
                    struct reexec_awaitable : stdcoro::suspend_always
                    {
                        void await_suspend(stdcoro::coroutine_handle<coroutine_promise> h)
                        {
                            coroutine_t{h}.resume_on_executor();
                        }
                    };
                    return reexec_awaitable{};
                }
            }

            template<typename... Args,typename Initiation,typename... InitArgs>
            auto await_transform(use_coroutine_awaitable<
                std::tuple<Args...>,Initiation,InitArgs...>&& a) const noexcept;

            decltype(auto) await_transform(auto&& a) const noexcept;
        };

        template<typename Coroutine>
        struct coroutine_awaitable;
    }

    template<coroutine_options Options,typename Result,executor Executor>
    class basic_coroutine : private coro_handle_owner<detail::coroutine_promise<
        Options&~coroutine_option::assume_blocking,Result,Executor>>
    {
        static_assert(std::is_void_v<Result>||
            (std::is_nothrow_move_constructible_v<Result>&&
             std::is_nothrow_move_assignable_v<Result>),
            "Result must be void or noexcept-moveable type");
    public:
        using promise_type = detail::coroutine_promise<
            Options&~coroutine_option::assume_blocking,Result,Executor>;
    private:
        using base_t = coro_handle_owner<promise_type>;
    public:
        using result_type = decltype(std::declval<promise_type>().result());
        using executor_type = Executor;
        using run_result_type = std::conditional_t<
            Options&coroutine_option::handle_exceptions,
            boost::outcome_v2::boost_result<result_type,std::exception_ptr>,
            result_type>;
        using completion_handler_sig = std::conditional_t<std::is_void_v<run_result_type>,
            void (),void (std::conditional_t<std::is_void_v<run_result_type>,int,run_result_type>)>;

        // rebind_executor is not provided because we store executor
        // in the promise that cannot be recreated.

        class iterator : public boost::stl_interfaces::iterator_interface<
            iterator,std::input_iterator_tag,Result>
        {
        public:
            iterator() noexcept = default;

            Result& operator*() const noexcept
            {
                return *(*gen_)->result();
            }

            iterator& operator++()
                noexcept(!(Options&coroutine_option::handle_exceptions))
            {
                gen_->operator co_await();
                if(!*gen_)
                    gen_ = {};
                return *this;
            }

            bool operator==(const iterator& other) const noexcept
            {
                return gen_==other.gen_;
            }

            bool operator!=(const iterator& other) const noexcept
            {
                return !(*this==other);
            }
        private:
            friend basic_coroutine;

            basic_coroutine* gen_ = {};

            iterator(basic_coroutine* gen) noexcept
                : gen_{gen}
            {}
        };

        constexpr static coroutine_options options = Options;

        basic_coroutine() noexcept = default;

        basic_coroutine<Options|coroutine_option::assume_blocking,Result,Executor>
        assume_blocking() &&
            requires (bool (Options&coroutine_option::save_awaiter))&&
                     (!(Options&coroutine_option::assume_blocking))
        {
            return {std::move(*this).release()};
        }

        explicit operator bool() const noexcept
        {
            if constexpr(bool(Options&coroutine_option::delegated_yield))
                return this->get()&&!current_subgen().done();
            else
                return base_t::operator bool();
        }

        executor_type get_executor() const noexcept
        {
            return (*this)->get_executor();
        }

        void set_executor(Executor new_executor) noexcept
        {
            (*this)->set_executor(std::move(new_executor));
        }

        result_type operator()()
            noexcept(!(Options&(coroutine_option::save_awaiter|coroutine_option::handle_exceptions)));

        auto operator co_await() noexcept((Options&coroutine_option::save_awaiter)||
                                          !(Options&coroutine_option::handle_exceptions));

        template<boost::asio::completion_token_for<completion_handler_sig>
            CompletionToken = boost::asio::default_completion_token_t<Executor>>
        auto async_run(CompletionToken&& token = {}) &&
        {
            return async_run_impl<basic_coroutine>(std::forward<CompletionToken>(token));
        }

        template<boost::asio::completion_token_for<completion_handler_sig>
            CompletionToken = boost::asio::default_completion_token_t<Executor>>
        auto async_run(CompletionToken&& token = {}) &
        {
            return async_run_impl<basic_coroutine&>(std::forward<CompletionToken>(token));
        }

        iterator begin()
            requires (!(Options&coroutine_option::save_awaiter)&&
                      bool(Options&coroutine_option::support_yield))
        {
            return std::next(iterator{this});
        }

        iterator end() noexcept
            requires (!(Options&coroutine_option::save_awaiter)&&
                      bool(Options&coroutine_option::support_yield))
        {
            return {};
        }
    private:
        template<coroutine_options OtherOptions,typename OtherResult,executor OtherExecutor>
        friend class basic_coroutine;

        template<coroutine_options OtherOptions,typename OtherResult,executor OtherExecutor>
        friend struct detail::coroutine_promise;

        template<coroutine_options OtherOptions,typename OtherResult>
        friend struct detail::coroutine_promise_result;

        friend detail::coroutine_awaitable<basic_coroutine>;

        basic_coroutine(promise_type& promise) noexcept
            : base_t{stdcoro::coroutine_handle<promise_type>::from_promise(promise)}
        {}

        basic_coroutine(stdcoro::coroutine_handle<promise_type> handle) noexcept
            : base_t{handle}
        {}

        void resume_on_executor() &&
        {
            auto ex = boost::asio::prefer((*this)->work_,
                                          boost::asio::execution::relationship.continuation);
            boost::asio::execution::execute(ex,[coro=std::move(*this)]() mutable {
                std::move(coro).release()();
            });
        }

        stdcoro::coroutine_handle<> current_subgen() const noexcept
        {
            using promise_t = detail::coroutine_promise<
                options&~coroutine_option::delegated_yield,Result,Executor>;
            return stdcoro::coroutine_handle<promise_t>::from_promise(
                    *static_cast<promise_t*>((*this)->current_));
        }

        template<typename This,
                 boost::asio::completion_token_for<completion_handler_sig> CompletionToken>
        auto async_run_impl(CompletionToken&& token)
        {
            return boost::asio::async_initiate<CompletionToken,completion_handler_sig>(
                [this](auto handler) mutable {
                    auto trampoline_ex = boost::asio::prefer(boost::asio::system_executor{},
                        boost::asio::execution::allocator(boost::asio::get_associated_allocator(
                            handler,(*this)->get_work_allocator())));
                    // Making this eager (initial_suspend -> suspend_never)
                    // gives slightly worse codegen.
                    [](auto trampoline_ex,This coro,auto handler)
                            -> basic_coroutine<coroutine_option::handle_exceptions|
                                               coroutine_option::orphan_,
                                               void,decltype(trampoline_ex)> {
                        auto ex = boost::asio::prefer(
                            boost::asio::get_associated_executor(handler,coro->work_),
                            boost::asio::execution::allocator(
                                boost::asio::query(trampoline_ex,boost::asio::execution::allocator)),
                            boost::asio::execution::outstanding_work.tracked
                        );
                        if constexpr(Options&coroutine_option::handle_exceptions){
                            auto cb = [&](run_result_type result) mutable {
                                boost::asio::execution::execute(std::move(ex),
                                    [handler=std::move(handler),
                                            result=std::move(result)]() mutable {
                                        handler(std::move(result));
                                    });
                            };
                            try{
                                if constexpr(std::is_void_v<result_type>){
                                    co_await coro;
                                    cb(boost::outcome_v2::success());
                                }else
                                    cb(boost::outcome_v2::success(co_await std::move(coro)));
                            }
                            catch(...){
                                cb(boost::outcome_v2::failure(std::current_exception()));
                            }
                        }else if constexpr(std::is_void_v<run_result_type>){
                            co_await coro;
                            boost::asio::execution::execute(std::move(ex),
                                [handler=std::move(handler)]() mutable {
                                    handler();
                                });
                        }else
                            boost::asio::execution::execute(std::move(ex),
                                [handler=std::move(handler),
                                        result=co_await coro]() mutable {
                                    handler(std::move(result));
                                });
                    }(trampoline_ex,static_cast<This&&>(*this),std::move(handler)).release()();
                },token);
        }
    };

    namespace detail
    {
        template<coroutine_options Options,typename Result,executor Executor>
        basic_coroutine<Options,Result,Executor> coroutine_promise<Options,Result,Executor>::
            get_return_object() noexcept
        {
            return {*this};
        }

        template<typename Coroutine>
        struct coroutine_awaitable : stdcoro::suspend_never
        {
            // We can't symmetrically call a coroutine that doesn't save
            // awaiter, since it would also have to return symmetrically.

            Coroutine* coro_;

            coroutine_awaitable(Coroutine* coro)
                    noexcept(!(Coroutine::options&coroutine_option::handle_exceptions))
                : coro_{coro}
            {
                if constexpr(bool(Coroutine::options&coroutine_option::delegated_yield)){
                    for(;;){
                        coro_->current_subgen()();
                        if((*coro_)->current_->result_)
                            break;
                        auto prev = (*coro_)->prev_;
                        if(!prev)
                            break;
                        (*coro_)->current_ = prev;
                        (*coro_)->prev_ = prev->prev_==prev?nullptr:prev->prev_;
                    }
                }else
                    coro_->get()();
            }

            typename Coroutine::result_type await_resume() noexcept
            {
                return (*coro_)->result();
            }
        };

        template<typename Coroutine>
            requires (bool(Coroutine::options&coroutine_option::save_awaiter))
        struct [[nodiscard]] coroutine_awaitable<Coroutine> : stdcoro::suspend_always
        {
            Coroutine* coro_;
            typename Coroutine::promise_type* promise_;

            coroutine_awaitable(Coroutine* coro) noexcept
                : coro_{coro}
            {}

            auto await_suspend(stdcoro::coroutine_handle<> awaiter_handle)
            {
                promise_ = coro_->operator->();
                promise_->awaiter_ = awaiter_handle;
                // We steal from coro_ to ensure it doesn't get destroyed a second
                // time when our handle is destroyed externally and *coro_ is in our promise.
                // We'll restore it in await_resume from a saved promise address.
                if constexpr(is_trivial_executor_v<typename Coroutine::executor_type>)
                    return std::move(*coro_).release();
                else
                    return std::move(*coro_).resume_on_executor();
            }

            typename Coroutine::result_type await_resume()
                noexcept(!(Coroutine::options&coroutine_option::handle_exceptions))
            {
                // We know we've stolen from coro_, so we placement-construct,
                // skipping destruction we know is trivial, instead of assigning.
                // Otherwise the compiler cannot optimize away "if(handle) destroy()" sequence.
                ::new (static_cast<void*>(coro_)) Coroutine{*promise_};
                return promise_->result();
            }
        };
    }

    template<typename Coroutine>
    struct is_asymmetric_awaitable<detail::coroutine_awaitable<Coroutine>>
        : std::bool_constant<!(Coroutine::options&coroutine_option::save_awaiter)||
                             (Coroutine::options&coroutine_option::subservant)> {};

    namespace detail
    {
        template<coroutine_options Options,typename Result,executor Executor>
        decltype(auto) coroutine_promise<Options,Result,Executor>::await_transform(auto&& a) const noexcept
        {
            using awaitable_t = std::remove_cvref_t<decltype(a)>;
            static_assert((Options&(coroutine_option::save_awaiter|
                                    coroutine_option::orphan_))||
                          is_asymmetric_awaitable_v<awaitable_t>,
                          "Symmetric awaitables are only supported by saving awaiter, "
                          "to use custom awaitables in coroutine, specialize is_asymmetric_awaitable.");
            if constexpr(is_trivial_executor_v<Executor>||
                         is_asymmetric_awaitable_v<awaitable_t>)
                return std::forward<decltype(a)>(a);
            else{
                struct transformed_awaiter : awaiter_wrapper<awaiter_type_t<decltype(a)>>
                {
                    auto await_suspend(stdcoro::coroutine_handle<coroutine_promise> handle)
                    {
                        return this->a_.await_suspend([](coroutine_t coro) mutable
                                -> basic_coroutine<coroutine_option::handle_exceptions|
                                                   coroutine_option::orphan_,
                                                   void,boost::asio::system_executor> {
                            std::move(coro).resume_on_executor();
                            co_return;
                        }(handle).release());
                    }
                };
                return transformed_awaiter{{get_awaiter(std::forward<decltype(a)>(a))}};
            }
        }

        inline auto make_latch_countdowner(std::latch& l) noexcept
        {
            constexpr auto latch_countdowner = [](std::latch* l){
                l->count_down();
            };
            return std::unique_ptr<std::latch,decltype(latch_countdowner)>{&l,latch_countdowner};
        }
    }

    template<coroutine_options Options,typename Result,executor Executor>
    auto basic_coroutine<Options,Result,Executor>::operator()()
        noexcept(!(Options&(coroutine_option::save_awaiter|coroutine_option::handle_exceptions)))
        -> result_type
    {
        if constexpr(bool(Options&coroutine_option::save_awaiter)){
            using stub_run_result_t = std::conditional_t<std::is_void_v<run_result_type>,
                                                         std::nullptr_t,run_result_type>;
            [[maybe_unused]] std::conditional_t<
                std::is_trivially_default_constructible_v<stub_run_result_t>,
                stub_run_result_t,std::optional<stub_run_result_t>> ret;
            bool killed = true;
            std::latch l{1};
            if constexpr(std::is_void_v<run_result_type>)
                async_run([&,lc=detail::make_latch_countdowner(l)]{
                    killed = false;
                });
            else
                async_run([&,lc=detail::make_latch_countdowner(l)]
                        (run_result_type res){
                    ret = std::move(res);
                    killed = false;
                });
            if constexpr(!(Options&coroutine_option::assume_blocking)){
                l.wait();
                if(killed)
                    throw boost::system::system_error{make_error_code(
                        boost::system::errc::operation_canceled)};
            }
            if constexpr(std::is_void_v<run_result_type>)
                return;
            else if constexpr(std::is_trivially_default_constructible_v<run_result_type>)
                return ret;
            else if constexpr(Options&coroutine_option::handle_exceptions)
                return ret->value();
            else
                return std::move(*ret);
        }else
            return operator co_await().await_resume();
    }

    template<coroutine_options Options,typename Result,executor Executor>
    auto basic_coroutine<Options,Result,Executor>::operator co_await()
        noexcept((Options&coroutine_option::save_awaiter)||
                 !(Options&coroutine_option::handle_exceptions))
    {
        return detail::coroutine_awaitable<basic_coroutine>{this};
    }
}

#endif
