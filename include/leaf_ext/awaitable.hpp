#pragma once

#include <algorithm>

#define LEAF

template <>
class asio::detail::awaitable_frame_base<leaf_ext::asio::executor>
{
public:
#if !defined(ASIO_DISABLE_AWAITABLE_FRAME_RECYCLING)
  void *operator new(std::size_t size)
  {
    return asio::detail::thread_info_base::allocate(
        asio::detail::thread_info_base::awaitable_frame_tag(),
        asio::detail::thread_context::top_of_thread_call_stack(),
        size);
  }

  void operator delete(void *pointer, std::size_t size)
  {
    asio::detail::thread_info_base::deallocate(
        asio::detail::thread_info_base::awaitable_frame_tag(),
        asio::detail::thread_context::top_of_thread_call_stack(),
        pointer, size);
  }
#endif // !defined(ASIO_DISABLE_AWAITABLE_FRAME_RECYCLING)

  // The frame starts in a suspended state until the awaitable_thread object
  // pumps the stack.
  auto initial_suspend() noexcept
  {
    return suspend_always();
  }

  // On final suspension the frame is popped from the top of the stack.
  auto final_suspend() noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return false;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
        this->this_->pop_frame();
      }

      void await_resume() const noexcept
      {
      }
    };

    return result{this};
  }

  void set_except(std::exception_ptr e) noexcept
  {
    pending_exception_ = e;
  }

  void set_error(const asio::error_code &ec)
  {
    this->set_except(std::make_exception_ptr(asio::system_error(ec)));
  }

  void unhandled_exception()
  {
    set_except(std::current_exception());
  }

  void rethrow_exception()
  {
    if (pending_exception_)
    {
      std::exception_ptr ex = std::exchange(pending_exception_, nullptr);
      std::rethrow_exception(ex);
    }
  }

  void clear_cancellation_slot();

  template <typename T>
  auto await_transform(awaitable<T, leaf_ext::asio::executor> a) const;

  leaf_ext::asio::executor get_executor() const noexcept;
  cancellation_state get_cancellation_state() const noexcept;
  void reset_cancellation_state() const;
  template <typename Filter>
  void reset_cancellation_state(ASIO_MOVE_ARG(Filter) filter);
  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(ASIO_MOVE_ARG(InFilter) in_filter,
                                ASIO_MOVE_ARG(OutFilter) out_filter);
  bool throw_if_cancelled() const;
  void throw_if_cancelled(bool value);
  awaitable_frame<awaitable_thread_entry_point, leaf_ext::asio::executor> *entry_point() const noexcept;

  // This await transformation obtains the associated executor of the thread of
  // execution.
  auto await_transform(this_coro::executor_t) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume() const noexcept
      {
        return this_->get_executor();
      }
    };

    return result{this};
  }

  // This await transformation obtains the associated cancellation state of the
  // thread of execution.
  auto await_transform(this_coro::cancellation_state_t) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume() const noexcept
      {
        return this_->get_cancellation_state();
      }
    };

    return result{this};
  }

  // This await transformation resets the associated cancellation state.
  auto await_transform(this_coro::reset_cancellation_state_0_t) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume() const
      {
        return this_->reset_cancellation_state();
      }
    };

    return result{this};
  }

  // This await transformation resets the associated cancellation state.
  template <typename Filter>
  auto await_transform(
      this_coro::reset_cancellation_state_1_t<Filter> reset) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;
      Filter filter_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        return this_->reset_cancellation_state(
            ASIO_MOVE_CAST(Filter)(filter_));
      }
    };

    return result{this, ASIO_MOVE_CAST(Filter)(reset.filter)};
  }

  // This await transformation resets the associated cancellation state.
  template <typename InFilter, typename OutFilter>
  auto await_transform(
      this_coro::reset_cancellation_state_2_t<InFilter, OutFilter> reset) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;
      InFilter in_filter_;
      OutFilter out_filter_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        return this_->reset_cancellation_state(
            ASIO_MOVE_CAST(InFilter)(in_filter_),
            ASIO_MOVE_CAST(OutFilter)(out_filter_));
      }
    };

    return result{this,
                  ASIO_MOVE_CAST(InFilter)(reset.in_filter),
                  ASIO_MOVE_CAST(OutFilter)(reset.out_filter)};
  }

  // This await transformation determines whether cancellation is propagated as
  // an exception.
  auto await_transform(this_coro::throw_if_cancelled_0_t) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        return this_->throw_if_cancelled();
      }
    };

    return result{this};
  }

  // This await transformation sets whether cancellation is propagated as an
  // exception.
  auto await_transform(this_coro::throw_if_cancelled_1_t throw_if_cancelled) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;
      bool value_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        this_->throw_if_cancelled(value_);
      }
    };

    return result{this, throw_if_cancelled.value};
  }

  // This await transformation is used to run an async operation's initiation
  // function object after the coroutine has been suspended. This ensures that
  // immediate resumption of the coroutine in another thread does not cause a
  // race condition.
  template <typename Function>
  auto await_transform(Function f,
                       typename enable_if<
                           is_convertible<
                               typename result_of<Function(awaitable_frame_base *)>::type,
                               awaitable_thread<leaf_ext::asio::executor> *>::value>::type * = 0)
  {
    struct result
    {
      Function function_;
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return false;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
        function_(this_);
      }

      void await_resume() const noexcept
      {
      }
    };

    return result{std::move(f), this};
  }

  // Access the awaitable thread's has_context_switched_ flag.
  auto await_transform(detail::awaitable_thread_has_context_switched) noexcept
  {
    struct result
    {
      awaitable_frame_base *this_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      bool &await_resume() const noexcept
      {
        return this_->entry_point()->has_context_switched_;
      }
    };

    return result{this};
  }

#if defined(LEAF)
  auto await_transform(boost::leaf::context_ptr &ctx) noexcept;
#endif // defined(LEAF)

  void attach_thread(awaitable_thread<leaf_ext::asio::executor> *handler) noexcept;
  awaitable_thread<leaf_ext::asio::executor> *detach_thread() noexcept;

  void push_frame(awaitable_frame_base<leaf_ext::asio::executor> *caller) noexcept;
  void pop_frame() noexcept;

  void resume()
  {
    coro_.resume();
  }

  void destroy()
  {
    coro_.destroy();
  }

protected:
  coroutine_handle<void> coro_ = nullptr;
  awaitable_thread<leaf_ext::asio::executor> *attached_thread_ = nullptr;
  awaitable_frame_base<leaf_ext::asio::executor> *caller_ = nullptr;
  std::exception_ptr pending_exception_ = nullptr;

#if defined(LEAF)
  boost::leaf::context_ptr ctx_;
#endif // defined(LEAF)
};

template <>
class asio::detail::awaitable_thread<leaf_ext::asio::executor>
{
public:
  typedef leaf_ext::asio::executor executor_type;
  typedef cancellation_slot cancellation_slot_type;

  // Construct from the entry point of a new thread of execution.
  awaitable_thread(awaitable<awaitable_thread_entry_point, leaf_ext::asio::executor> p,
                   const leaf_ext::asio::executor &ex, cancellation_slot parent_cancel_slot,
                   cancellation_state cancel_state)
      : bottom_of_stack_(std::move(p))
  {
    bottom_of_stack_.frame_->top_of_stack_ = bottom_of_stack_.frame_;
    new (&bottom_of_stack_.frame_->u_.executor_) leaf_ext::asio::executor(ex);
    bottom_of_stack_.frame_->has_executor_ = true;
    bottom_of_stack_.frame_->parent_cancellation_slot_ = parent_cancel_slot;
    bottom_of_stack_.frame_->cancellation_state_ = cancel_state;
  }

  // Transfer ownership from another awaitable_thread.
  awaitable_thread(awaitable_thread &&other) noexcept
      : bottom_of_stack_(std::move(other.bottom_of_stack_))
#if defined(LEAF)
        ,
        ctx_(std::move(other.ctx_)),
        contexts_(other.contexts_)
#endif // defined(LEAF)
  {
  }

  // Clean up with a last ditch effort to ensure the thread is unwound within
  // the context of the executor.
  ~awaitable_thread()
  {
    if (bottom_of_stack_.valid())
    {
      // Coroutine "stack unwinding" must be performed through the executor.
      auto *bottom_frame = bottom_of_stack_.frame_;
      (post)(bottom_frame->u_.executor_,
             [a = std::move(bottom_of_stack_)]() mutable
             {
               (void)awaitable<awaitable_thread_entry_point, leaf_ext::asio::executor>(
                   std::move(a));
             });
    }
  }

  awaitable_frame<awaitable_thread_entry_point, leaf_ext::asio::executor> *entry_point()
  {
    return bottom_of_stack_.frame_;
  }

  executor_type get_executor() const noexcept
  {
    return bottom_of_stack_.frame_->u_.executor_;
  }

  cancellation_state get_cancellation_state() const noexcept
  {
    return bottom_of_stack_.frame_->cancellation_state_;
  }

  void reset_cancellation_state()
  {
    bottom_of_stack_.frame_->cancellation_state_ =
        cancellation_state(bottom_of_stack_.frame_->parent_cancellation_slot_);
  }

  template <typename Filter>
  void reset_cancellation_state(ASIO_MOVE_ARG(Filter) filter)
  {
    bottom_of_stack_.frame_->cancellation_state_ =
        cancellation_state(bottom_of_stack_.frame_->parent_cancellation_slot_,
                           ASIO_MOVE_CAST(Filter)(filter));
  }

  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(ASIO_MOVE_ARG(InFilter) in_filter,
                                ASIO_MOVE_ARG(OutFilter) out_filter)
  {
    bottom_of_stack_.frame_->cancellation_state_ =
        cancellation_state(bottom_of_stack_.frame_->parent_cancellation_slot_,
                           ASIO_MOVE_CAST(InFilter)(in_filter),
                           ASIO_MOVE_CAST(OutFilter)(out_filter));
  }

  bool throw_if_cancelled() const
  {
    return bottom_of_stack_.frame_->throw_if_cancelled_;
  }

  void throw_if_cancelled(bool value)
  {
    bottom_of_stack_.frame_->throw_if_cancelled_ = value;
  }

  cancellation_slot_type get_cancellation_slot() const noexcept
  {
    return bottom_of_stack_.frame_->cancellation_state_.slot();
  }

  // Launch a new thread of execution.
  void launch()
  {
    bottom_of_stack_.frame_->top_of_stack_->attach_thread(this);
    pump();
  }

#if defined(LEAF)
  void push_context(boost::leaf::context_ptr context)
  {
    contexts_.push_back(context);
  }

  void pop_context(boost::leaf::context_ptr context)
  {
    assert(contexts_.back() == context);
    contexts_.pop_back();
  }
#endif // defined(LEAF)

protected:
  template <typename>
  friend class awaitable_frame_base;

  // Repeatedly resume the top stack frame until the stack is empty or until it
  // has been transferred to another resumable_thread object.
  void pump()
  {
#if defined(LEAF)
    std::for_each(contexts_.begin(), contexts_.end(), [](auto &&context)
                  { context->activate(); });
#endif // defined(LEAF)
    do
      bottom_of_stack_.frame_->top_of_stack_->resume();
    while (bottom_of_stack_.frame_ && bottom_of_stack_.frame_->top_of_stack_);
#if defined(LEAF)
    std::for_each(contexts_.rbegin(), contexts_.rend(), [](auto &&context)
                  { context->deactivate(); });
#endif // defined(LEAF)

    if (bottom_of_stack_.frame_)
    {
      awaitable<awaitable_thread_entry_point, leaf_ext::asio::executor> a(
          std::move(bottom_of_stack_));
      a.frame_->rethrow_exception();
    }
  }

  awaitable<awaitable_thread_entry_point, leaf_ext::asio::executor> bottom_of_stack_;

#if defined(LEAF)
  boost::leaf::context_ptr ctx_ = boost::leaf::make_shared_context(
      [](const boost::leaf::error_info &)
      {
        std::terminate();
      }); // context without any handler
  std::vector<boost::leaf::context_ptr> contexts_;
#endif // defined(LEAF)
};

inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::clear_cancellation_slot()
{
  assert(this->attached_thread_);
  this->attached_thread_->entry_point()->cancellation_state_.slot().clear();
}

template <typename T>
inline auto asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::await_transform(awaitable<T, leaf_ext::asio::executor> a) const
{
  assert(attached_thread_);
  if (attached_thread_->entry_point()->throw_if_cancelled_)
    if (!!attached_thread_->get_cancellation_state().cancelled())
      do_throw_error(asio::error::operation_aborted, "co_await");
  return a;
}

inline leaf_ext::asio::executor asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::get_executor() const noexcept
{
  assert(attached_thread_);
  return attached_thread_->get_executor();
}

inline asio::cancellation_state asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::get_cancellation_state() const noexcept
{
  assert(attached_thread_);
  return attached_thread_->get_cancellation_state();
}

inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::reset_cancellation_state() const
{
  assert(attached_thread_);
  attached_thread_->reset_cancellation_state();
}

template <typename Filter>
inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::reset_cancellation_state(ASIO_MOVE_ARG(Filter) filter)
{
  assert(attached_thread_);
  attached_thread_->reset_cancellation_state(std::forward<Filter>(filter));
}

template <typename InFilter, typename OutFilter>
inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::reset_cancellation_state(ASIO_MOVE_ARG(InFilter) in_filter,
                              ASIO_MOVE_ARG(OutFilter) out_filter)
{
  assert(attached_thread_);
  attached_thread_->reset_cancellation_state(std::forward<InFilter>(in_filter), std::forward<OutFilter>(out_filter));
}

inline bool asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::throw_if_cancelled() const
{
  assert(attached_thread_);
  return attached_thread_->throw_if_cancelled();
}

inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::throw_if_cancelled(bool value)
{
  assert(attached_thread_);
  attached_thread_->throw_if_cancelled(value);
}

inline asio::detail::awaitable_frame<asio::detail::awaitable_thread_entry_point, leaf_ext::asio::executor> *asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::entry_point() const noexcept
{
  assert(attached_thread_);
  return attached_thread_->entry_point();
}

inline auto asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::await_transform(boost::leaf::context_ptr &ctx) noexcept
{
  assert(attached_thread_);

  struct result
  {
    boost::leaf::context_ptr ctx_;
    awaitable_frame_base<leaf_ext::asio::executor> *frame_;

    bool await_ready() const noexcept { return true; }

    void await_suspend(asio::detail::coroutine_handle<void>) noexcept {}

    auto await_resume() const noexcept
    {
      assert(!ctx_ || !frame_->ctx_);
      if (ctx_)
        frame_->attached_thread_->push_context(ctx_);
      else if (frame_->ctx_)
        frame_->attached_thread_->pop_context(frame_->ctx_);
      return frame_->ctx_ = ctx_;
    }
  };

  return result{ctx, this};
}

inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::attach_thread(awaitable_thread<leaf_ext::asio::executor> *handler) noexcept
{
  assert(handler);
  attached_thread_ = handler;
}

inline asio::detail::awaitable_thread<leaf_ext::asio::executor> *asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::detach_thread() noexcept
{
  assert(attached_thread_);
  attached_thread_->entry_point()->has_context_switched_ = true;
  return std::exchange(attached_thread_, nullptr);
}

inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::push_frame(awaitable_frame_base<leaf_ext::asio::executor> *caller) noexcept
{
  assert(caller && caller->attached_thread_);
  caller_ = caller;
  attached_thread_ = caller_->attached_thread_;
  attached_thread_->entry_point()->top_of_stack_ = this;
  caller_->attached_thread_ = nullptr;
}

inline void asio::detail::awaitable_frame_base<leaf_ext::asio::executor>::pop_frame() noexcept
{
  assert(attached_thread_);
  if (caller_)
    caller_->attached_thread_ = attached_thread_;
  attached_thread_->entry_point()->top_of_stack_ = caller_;
  attached_thread_ = nullptr;
  caller_ = nullptr;
}
