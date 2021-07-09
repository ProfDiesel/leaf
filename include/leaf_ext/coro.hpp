#pragma once

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

#include <boost/leaf/context.hpp>
#include <boost/leaf/handle_errors.hpp>

#include <functional>
#include <unordered_map>

namespace leaf_ext::asio {
using parent_executor = ::asio::any_io_executor;
struct executor : parent_executor {
  using parent_executor::parent_executor;
};

template <typename value_type>
using awaitable = ::asio::awaitable<value_type, executor>;

constexpr ::asio::use_awaitable_t<executor> use_awaitable(0, 0, 0);

} // namespace leaf_ext::asio

#include <leaf_ext/awaitable.hpp>

namespace leaf_ext::asio {

template<typename awaitable_type, typename value_type>
using rebind_awaitable_value_t = ::asio::awaitable<value_type, typename awaitable_type::executor_type>;

template <class TryBlock, class... H>
inline
rebind_awaitable_value_t<decltype(std::declval<TryBlock>()()), std::decay_t<decltype(std::declval<TryBlock>()().await_resume().value())>>
co_try_handle_all(TryBlock &&try_block, H &&...h) noexcept
{
    static_assert(boost::leaf::is_result_type<decltype(std::declval<TryBlock>()().await_resume())>::value, "The return type of the try_block passed to a co_try_handle_all function must be registered with leaf::is_result_type");
    auto ctx_ptr = boost::leaf::make_shared_context(std::forward<H>(h)...);
    auto &ctx = static_cast<boost::leaf::leaf_detail::polymorphic_context_impl<boost::leaf::context_type_from_handlers<H...>>&>(*ctx_ptr);

    co_await ctx_ptr;
    ctx.activate();

    if (auto r = co_await std::forward<TryBlock>(try_block)())
    {
        ctx.deactivate();
        ctx_ptr.reset();
        co_await ctx_ptr;

        co_return std::move(r).value();
    }
    else
    {
        boost::leaf::error_id id = r.error();

        ctx.deactivate();
        ctx_ptr.reset();
        co_await ctx_ptr;

        using R = std::decay_t<decltype(std::declval<TryBlock>()().await_resume().value())>;
        if constexpr(std::is_void_v<R>)
            ctx.template handle_error<R>(std::move(id), std::forward<H>(h)...);
        else
            co_return ctx.template handle_error<R>(std::move(id), std::forward<H>(h)...);
    }
}

template <class TryBlock, class... H>
BOOST_LEAF_NODISCARD inline
rebind_awaitable_value_t<decltype(std::declval<TryBlock>()()), std::decay_t<decltype(std::declval<TryBlock>()().await_resume())>>
co_try_handle_some( TryBlock && try_block, H && ... h ) noexcept
{
    static_assert(boost::leaf::is_result_type<decltype(std::declval<TryBlock>()().await_resume())>::value, "The return type of the try_block passed to a co_try_handle_some function must be registered with leaf::is_result_type");
    boost::leaf::context_type_from_handlers<H...> ctx;
    auto active_context = boost::leaf::activate_context(ctx);
    if( auto r = co_await std::forward<TryBlock>(try_block)() )
        co_return r;
    else
    {
        boost::leaf::error_id id = r.error();
        ctx.deactivate();
        using R = std::decay_t<decltype(std::declval<TryBlock>()().await_resume())>;
        auto rr = ctx.template handle_error<R>(std::move(id), std::forward<H>(h)..., [&r]()->R { return std::move(r); });
        if( !rr )
            ctx.propagate();
        co_return rr;
    }
}

} // namespace leaf_ext::asio
