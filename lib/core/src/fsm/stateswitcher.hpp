#ifndef FSM_STATESWITCHER_HPP
#define FSM_STATESWITCHER_HPP

#include "ctrl/timer.hpp"
#include "fsm/context.hpp"

#include "utils/task.hpp"

namespace fsm {

template <typename S, typename... Args>
inline cr::DetachedHandle SwitchToState(Context ctx, Args... args)
{
   co_await ctx.timer->WaitFor(std::chrono::milliseconds(0));
   // TODO: rethrow exceptions
   if (!std::holds_alternative<S>(*ctx.state))
      ctx.state->template emplace<S>(ctx, std::move(args)...);
}

template <>
inline cr::DetachedHandle SwitchToState<std::monostate>(Context ctx)
{
   co_await ctx.timer->WaitFor(std::chrono::milliseconds(0));
   // TODO: rethrow exceptions
   ctx.state->template emplace<std::monostate>();
}

} // namespace fsm

#endif
