#pragma once

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>

#include <boost/leaf/context.hpp>
#include <boost/leaf/handle_errors.hpp>

#include <functional>
#include <unordered_map>

namespace boost::leaf {
using parent_executor = asio::any_io_executor;
struct executor : parent_executor {
  using parent_executor::parent_executor;
};

template <typename value_type>
using awaitable = asio::awaitable<value_type, executor>;

constexpr asio::use_awaitable_t<executor> use_awaitable(0, 0, 0);

} // namespace boost::leaf

namespace asio {

template<typename awaitable_type, typename value_type>
using rebind_awaitable_value_t = asio::awaitable<value_type, typename awaitable_type::executor_type>;

}

namespace asio {
namespace detail {
template <> class awaitable_thread<boost::leaf::executor>;

boost::leaf::executor get_executor(awaitable_thread<boost::leaf::executor> *);

template <> class awaitable_frame_base<boost::leaf::executor> {
public:
  typedef boost::leaf::executor executor_type;

#if !defined(ASIO_DISABLE_AWAITABLE_FRAME_RECYCLING)
  void *operator new(std::size_t size) {
    return asio::detail::thread_info_base::allocate(
        asio::detail::thread_info_base::awaitable_frame_tag(),
        asio::detail::thread_context::top_of_thread_call_stack(), size);
  }

  void operator delete(void *pointer, std::size_t size) {
    asio::detail::thread_info_base::deallocate(
        asio::detail::thread_info_base::awaitable_frame_tag(),
        asio::detail::thread_context::top_of_thread_call_stack(), pointer,
        size);
  }
#endif // !defined(ASIO_DISABLE_AWAITABLE_FRAME_RECYCLING)

  // The frame starts in a suspended state until the awaitable_thread object
  // pumps the stack.
  auto initial_suspend() noexcept { return suspend_always(); }

  // On final suspension the frame is popped from the top of the stack.
  auto final_suspend() noexcept {
    struct result {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept { return false; }

      void await_suspend(coroutine_handle<void>) noexcept {
        this->this_->pop_frame();
      }

      void await_resume() const noexcept {}
    };

    return result{this};
  }

  void set_except(std::exception_ptr e) noexcept { pending_exception_ = e; }

  void set_error(const asio::error_code &ec) {
    this->set_except(std::make_exception_ptr(asio::system_error(ec)));
  }

  void unhandled_exception() { set_except(std::current_exception()); }

  void rethrow_exception() {
    if (pending_exception_) {
      std::exception_ptr ex = std::exchange(pending_exception_, nullptr);
      std::rethrow_exception(ex);
    }
  }

  template <typename T> auto await_transform(awaitable<T, executor_type> a) const {
    return a;
  }

// LEAF
  /*
  template <typename T> auto await_transform(awaitable<boost::leaf::result<T>, executor_type> a) const {
    struct result {
      awaitable<leaf::result<T>, executor_type> a_;
      awaitable_frame_base *this_;

      bool await_ready() const noexcept { return false; }

      template<typename U>
      void await_suspend(coroutine_handle<awaitable_frame<U, executor_type> h) noexcept {}

      leaf::result<T> await_resume() const noexcept {
        return result_of_body(); 
      }
    };

    return result{a, this};
  }
  */
// LEAF

  // This await transformation obtains the associated executor of the thread of
  // execution.
  auto await_transform(this_coro::executor_t) noexcept {
    struct result {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept { return true; }

      void await_suspend(coroutine_handle<void>) noexcept {}

      auto await_resume() const noexcept {
        return get_executor(this_->attached_thread_);
      }
    };

    return result{this};
  }

  // This await transformation is used to run an async operation's initiation
  // function object after the coroutine has been suspended. This ensures that
  // immediate resumption of the coroutine in another thread does not cause a
  // race condition.
  template <typename Function>
  auto await_transform(
      Function f,
      typename enable_if<is_convertible<
          typename result_of<Function(awaitable_frame_base *)>::type,
          awaitable_thread<executor_type> *>::value>::type * = 0) {
    struct result {
      Function function_;
      awaitable_frame_base *this_;

      bool await_ready() const noexcept { return false; }

      void await_suspend(coroutine_handle<void>) noexcept { function_(this_); }

      void await_resume() const noexcept {}
    };

    return result{std::move(f), this};
  }

// LEAF
  auto await_transform(boost::leaf::context_ptr &ctx) noexcept;
// LEAF

  void attach_thread(awaitable_thread<executor_type> *handler) noexcept {
    attached_thread_ = handler;
  }

  awaitable_thread<executor_type> *detach_thread() noexcept {
    return std::exchange(attached_thread_, nullptr);
  }

  void push_frame(awaitable_frame_base<executor_type> *caller) noexcept;
  void pop_frame() noexcept;

  void resume() {
// LEAF
//    auto _ = ctx_ ? std::optional(boost::leaf::context_activator(*ctx_)) : std::nullopt;
// LEAF

    coro_.resume();
  }

  void destroy() { coro_.destroy(); }

//protected:
  coroutine_handle<void> coro_ = nullptr;
  awaitable_thread<executor_type> *attached_thread_ = nullptr;
  awaitable_frame_base<executor_type> *caller_ = nullptr;
  std::exception_ptr pending_exception_ = nullptr;

// LEAF
  boost::leaf::context_ptr ctx_;
// LEAF
};

template <> class awaitable_thread<boost::leaf::executor> {
public:
  typedef boost::leaf::executor executor_type;

  // Construct from the entry point of a new thread of execution.
  awaitable_thread(awaitable<void, executor_type> p, const executor_type &ex)
      : bottom_of_stack_(std::move(p)), top_of_stack_(bottom_of_stack_.frame_),
        executor_(ex) {}

  // Transfer ownership from another awaitable_thread.
  awaitable_thread(awaitable_thread &&other) noexcept
      : bottom_of_stack_(std::move(other.bottom_of_stack_)),
        top_of_stack_(std::exchange(other.top_of_stack_, nullptr)),
        executor_(std::move(other.executor_))
// LEAF
        ,
        ctx_(std::move(other.ctx_)),
        contexts_(other.contexts_)
        {}
// LEAF

  // Clean up with a last ditch effort to ensure the thread is unwound within
  // the context of the executor.
  ~awaitable_thread() {
    if (bottom_of_stack_.valid()) {
      // Coroutine "stack unwinding" must be performed through the executor.
      (post)(executor_, [a = std::move(bottom_of_stack_)]() mutable {
        awaitable<void, executor_type>(std::move(a));
      });
    }
  }

  executor_type get_executor() const noexcept { return executor_; }

  // Launch a new thread of execution.
  void launch() {
    top_of_stack_->attach_thread(this);
    pump();
  }

  void push_context(boost::leaf::context_ptr context)
  {
    contexts_.push_back(context);
  }

  void pop_context(boost::leaf::context_ptr context)
  {
    assert(contexts_.back() == context);
    contexts_.pop_back();
  }

protected:
  template <typename> friend class awaitable_frame_base;

  // Repeatedly resume the top stack frame until the stack is empty or until it
  // has been transferred to another resumable_thread object.
  void pump() {
// LEAF
    std::for_each(contexts_.begin(), contexts_.end(), [](auto &&context) { context->activate(); });
// LEAF

    do
      top_of_stack_->resume();
    while (top_of_stack_);

// LEAF
    std::for_each(contexts_.rbegin(), contexts_.rend(), [](auto &&context) { context->deactivate(); });
// LEAF

    if (bottom_of_stack_.valid()) {
      awaitable<void, executor_type> a(std::move(bottom_of_stack_));
      a.frame_->rethrow_exception();
    }
  }

  awaitable<void, executor_type> bottom_of_stack_;
  awaitable_frame_base<executor_type> *top_of_stack_;
  executor_type executor_;

// LEAF
  boost::leaf::context_ptr ctx_ = boost::leaf::make_shared_context_for_stack(
      [](const boost::leaf::error_info &) {
        std::terminate();
      }); // context without any handler, so that the whole stack of contexts in
          // the thread can be poped at once
  std::vector<boost::leaf::context_ptr> contexts_;
// LEAF
};

void awaitable_frame_base<boost::leaf::executor>::push_frame(awaitable_frame_base<executor_type> *caller) noexcept {
  caller_ = caller;
  attached_thread_ = caller_->attached_thread_;
  attached_thread_->top_of_stack_ = this;
  caller_->attached_thread_ = nullptr;

// LEAF
  if(ctx_)
    ctx_->activate();
// LEAF
}

void awaitable_frame_base<boost::leaf::executor>::pop_frame() noexcept {
  if (caller_)
    caller_->attached_thread_ = attached_thread_;
  attached_thread_->top_of_stack_ = caller_;
  attached_thread_ = nullptr;
  caller_ = nullptr;

// LEAF
  if(ctx_)
    ctx_->deactivate();
// LEAF
}

boost::leaf::executor get_executor(awaitable_thread<boost::leaf::executor> *thread)
{
  return thread->get_executor();
}

// LEAF
auto awaitable_frame_base<boost::leaf::executor>::await_transform(boost::leaf::context_ptr &ctx) noexcept {
  assert(attached_thread_);

  struct result {
    boost::leaf::context_ptr ctx_;
    awaitable_frame_base<executor_type> *frame_;

    bool await_ready() const noexcept { return true; }

    void await_suspend(asio::detail::coroutine_handle<void>) noexcept {}

    auto await_resume() const noexcept {
      assert(!ctx_ || !frame_->ctx_);
      if(ctx_)
        frame_->attached_thread_->push_context(ctx_);
      else if(frame_->ctx_)
        frame_->attached_thread_->pop_context(frame_->ctx_);
      return frame_->ctx_ = ctx_;
    }
  };

  return result{ctx, this};
}
// LEAF

} // namespace detail
} // namespace asio

namespace boost::leaf {

// template <class TryBlock, class... H>
// awaitable<void> co_try_handle_all(TryBlock &&try_block, H &&...h) noexcept {
//   std::tuple hs{std::forward<H>(h)...};
//   auto ctx_ptr = co_await leaf::make_shared_context(hs);
//   if (auto r = co_await std::forward<TryBlock>(try_block)())
//     co_return std::move(r).value();
//   else {
//     ctx_ptr->captured_id_ = r.error();
//     co_return try_handle_all([&]() { return result<void>(std::move(ctx_ptr)); },
//                              hs);
//   }
// }

template <class TryBlock, class... H>
inline 
asio::rebind_awaitable_value_t<decltype(std::declval<TryBlock>()()), std::decay_t<decltype(std::declval<TryBlock>()().await_resume().value())>>
co_try_handle_all(TryBlock &&try_block, H &&...h) noexcept
{
    static_assert(is_result_type<decltype(std::declval<TryBlock>()().await_resume())>::value, "The return type of the try_block passed to a co_try_handle_all function must be registered with leaf::is_result_type");
    auto ctx_ptr = make_shared_context(std::forward<H>(h)...);
    auto &ctx = static_cast<leaf_detail::polymorphic_context_impl<context_type_from_handlers<H...>>&>(*ctx_ptr);

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
        error_id id = r.error();

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
asio::rebind_awaitable_value_t<decltype(std::declval<TryBlock>()()), std::decay_t<decltype(std::declval<TryBlock>()().await_resume())>>
co_try_handle_some( TryBlock && try_block, H && ... h ) noexcept
{
    static_assert(is_result_type<decltype(std::declval<TryBlock>()().await_resume())>::value, "The return type of the try_block passed to a co_try_handle_some function must be registered with leaf::is_result_type");
    context_type_from_handlers<H...> ctx;
    auto active_context = activate_context(ctx);
    if( auto r = co_await std::forward<TryBlock>(try_block)() )
        co_return r;
    else
    {
        error_id id = r.error();
        ctx.deactivate();
        using R = std::decay_t<decltype(std::declval<TryBlock>()().await_resume())>;
        auto rr = ctx.template handle_error<R>(std::move(id), std::forward<H>(h)..., [&r]()->R { return std::move(r); });
        if( !rr )
            ctx.propagate();
        co_return rr;
    }
}

} // namespace boost::leaf
