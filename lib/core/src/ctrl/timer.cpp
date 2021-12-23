#include "ctrl/timer.hpp"

#include <algorithm>
#include <cassert>

using namespace std::chrono_literals;

namespace core {

struct Timer::FutureTimeout
{
   FutureTimeout(Timer & timer, std::chrono::milliseconds timeout)
      : m_timer(timer)
      , m_timeout(timeout)
   {}
   bool await_ready() const noexcept { return false; }
   void await_suspend(stdcr::coroutine_handle<> h) const
   {
      assert(m_timer.m_scheduler);
      m_timer.m_scheduler(h, std::max(0ms, m_timeout));
   }
   Timeout await_resume() const noexcept { return {}; }

private:
   Timer & m_timer;
   const std::chrono::milliseconds m_timeout;
};


cr::TaskHandle<Timeout> Timer::WaitFor(std::chrono::milliseconds delay)
{
   co_return co_await FutureTimeout(*this, delay);
}

} // namespace async
