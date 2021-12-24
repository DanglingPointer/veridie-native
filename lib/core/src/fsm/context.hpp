#ifndef FSM_CONTEXT_HPP
#define FSM_CONTEXT_HPP

#include "ctrl/commandadapter.hpp"
#include "ctrl/timer.hpp"
#include "fsm/statebase.hpp"

#include "utils/task.hpp"

#include <memory>

namespace dice {
class IEngine;
class ISerializer;
} // namespace dice

namespace fsm {

struct Context
{
   template <typename S, typename... Args>
   static cr::DetachedHandle SwitchToState(Context ctx, Args... args);

   Context(dice::IEngine * generator,
           dice::ISerializer * serializer,
           core::Timer * timer,
           core::CommandAdapter proxy,
           std::unique_ptr<StateBase> * stateHolder)
      : generator(generator)
      , serializer(serializer)
      , timer(timer)
      , proxy(proxy)
      , stateHolder(*stateHolder)
   {}

   dice::IEngine * const generator;
   dice::ISerializer * const serializer;
   core::Timer * const timer;
   core::CommandAdapter proxy;

private:
   std::unique_ptr<StateBase> & stateHolder;
};

template <typename S, typename... Args>
inline cr::DetachedHandle Context::SwitchToState(Context ctx, Args... args)
{
   co_await ctx.timer->WaitFor(std::chrono::milliseconds(0));

   if (auto * state = ctx.stateHolder.get(); state && typeid(*state) == typeid(S))
      co_return;

   ctx.stateHolder.reset();
   ctx.stateHolder = std::make_unique<S>(ctx, std::move(args)...);
}

template <>
inline cr::DetachedHandle Context::SwitchToState<void>(Context ctx)
{
   co_await ctx.timer->WaitFor(std::chrono::milliseconds(0));
   ctx.stateHolder.reset();
}

} // namespace fsm

#endif // FSM_CONTEXT_HPP
