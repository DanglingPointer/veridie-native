#ifndef TIMER_HPP
#define TIMER_HPP

#include "utils/task.hpp"

#include <chrono>
#include <functional>

namespace core {

struct Timeout
{};

class Timer
{
public:
   template <typename S>
   explicit Timer(S && scheduler)
      : m_scheduler(std::forward<S>(scheduler))
   {}

   cr::TaskHandle<Timeout> WaitFor(std::chrono::milliseconds delay);

private:
   struct FutureTimeout;

   using Task = std::function<void()>;
   const std::function<void(Task &&, std::chrono::milliseconds)> m_scheduler;
};

} // namespace async

#endif
