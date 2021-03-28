// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef UUID_81FAC294_C9BB_4C42_B4A7_9814C4A7A9F7
#define UUID_81FAC294_C9BB_4C42_B4A7_9814C4A7A9F7

#include <ampi/asio/with_as_default_on.hpp>

#include <boost/asio/async_result.hpp>

#include <tuple>

namespace ampi
{
    // boost::asio::experimental::as_single tries to define result_type
    // which is incompatible with initiate with argument-dependent
    // return type.

    namespace detail
    {
        template<typename T>
        struct tuple_sig_for;

        template<typename Ret,typename... Args>
        struct tuple_sig_for<Ret (Args...)>
        {
            using type = Ret (std::tuple<std::decay_t<Args>...>);
        };
    }

    template<typename CompletionToken>
    struct as_tuple_t : with_as_default_on<as_tuple_t<CompletionToken>>
    {
        CompletionToken token_;
    };

    template<typename CompletionToken>
    as_tuple_t<std::decay_t<CompletionToken>> as_tuple(CompletionToken&& token)
    {
        return {{},std::forward<CompletionToken>(token)};
    }
}

template<typename CompletionToken,typename Signature>
class boost::asio::async_result<ampi::as_tuple_t<CompletionToken>,Signature>
{
public:
    template<typename Initiation,typename RawCompletionToken,typename... Args>
    static auto initiate(Initiation&& init,RawCompletionToken&& token,Args&&... args)
    {
        return boost::asio::async_initiate<CompletionToken,
                typename ampi::detail::tuple_sig_for<Signature>::type>(
            [init=std::forward<Initiation>(init)](auto&& handler,auto&&... args) mutable {
                std::forward<Initiation>(init)(
                    [handler=std::forward<decltype(handler)>(handler)](auto&&... args) mutable {
                        std::forward<decltype(handler)>(handler)(
                            std::make_tuple(std::forward<decltype(args)>(args)...));
                    },
                    std::forward<decltype(args)>(args)...);
            },
        token.token_,std::forward<Args>(args)...);
    }
};

#endif
