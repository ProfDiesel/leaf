#pragma once

#include <boost/leaf/result.hpp>

#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/detail/handler_alloc_helpers.hpp>
#include <asio/detail/handler_cont_helpers.hpp>
#include <asio/detail/handler_invoke_helpers.hpp>

#include <system_error>
#include <type_traits>
#include <utility>

namespace boost::leaf
{


template<typename CompletionToken>
struct as_result_t {
    CompletionToken token_;
};

template<typename CompletionToken>
inline as_result_t<std::decay_t<CompletionToken>>
as_result(CompletionToken&& completion_token)
{
    return as_result_t<std::decay_t<CompletionToken>>{
        std::forward<CompletionToken>(completion_token)};
}

namespace detail {

// Class to adapt as_result_t as a completion handler
template<typename Handler>
struct leaf_result_handler {
    void operator()(const std::error_code& ec)
    {
        if (ec)
            handler_(ec);
        else
            handler_(boost::leaf::result<void>{});
    }

    template<typename T>
    void operator()(const std::error_code& ec, T t)
    {
        if (ec)
            handler_(ec);
        else
            handler_(boost::leaf::result<T>(std::move(t)));
    }

    Handler handler_;
};

template<typename Handler>
inline bool asio_handler_is_continuation(leaf_result_handler<Handler>* this_handler)
{
    return asio_handler_cont_helpers::is_continuation(this_handler->handler_);
}

template<typename Signature>
struct result_signature;

template<>
struct result_signature<void(std::error_code)> {
    using type = void(boost::leaf::result<void>);
};

template<>
struct result_signature<void(const std::error_code&)>
    : result_signature<void(std::error_code)> {};

template<typename T>
struct result_signature<void(std::error_code, T)> {
    using type = void(boost::leaf::result<T>);
};

template<typename T>
struct result_signature<void(const std::error_code&, T)>
    : result_signature<void(std::error_code, T)> {};

template<typename Signature>
using result_signature_t = typename result_signature<Signature>::type;

} // namespace detail

}

namespace asio {

template<typename CompletionToken, typename Signature>
class async_result<boost::leaf::as_result_t<CompletionToken>, Signature> {
public:
    using result_signature = boost::leaf::detail::result_signature_t<Signature>;

    using return_type =
        typename async_result<CompletionToken, result_signature>::return_type;

    template<typename Initiation, typename... Args>
    static return_type
    initiate(Initiation&& initiation, boost::leaf::as_result_t<CompletionToken>&& token, Args&&... args)
    {
        return async_initiate<CompletionToken, result_signature>(
            [init = std::forward<Initiation>(initiation)](
                auto&& handler, auto&&... callArgs) mutable {
                std::move(init)(
                    boost::leaf::detail::leaf_result_handler<std::decay_t<decltype(handler)>>{
                        std::forward<decltype(handler)>(handler)},
                    std::forward<decltype(callArgs)>(callArgs)...);
            },
            token.token_,
            std::forward<Args>(args)...);
    }
};

template<typename Handler, typename Executor>
struct associated_executor<boost::leaf::detail::leaf_result_handler<Handler>, Executor> {
    typedef typename associated_executor<Handler, Executor>::type type;

    static type
    get(const boost::leaf::detail::leaf_result_handler<Handler>& h,
        const Executor& ex = Executor()) noexcept
    {
        return associated_executor<Handler, Executor>::get(h.handler_, ex);
    }
};

template<typename Handler, typename Allocator>
struct associated_allocator<boost::leaf::detail::leaf_result_handler<Handler>, Allocator> {
    typedef typename associated_allocator<Handler, Allocator>::type type;

    static type
    get(const boost::leaf::detail::leaf_result_handler<Handler>& h,
        const Allocator& a = Allocator()) noexcept
    {
        return associated_allocator<Handler, Allocator>::get(h.handler_, a);
    }
};

} // namespace asio
