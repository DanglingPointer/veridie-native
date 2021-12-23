#include "mainexec.hpp"
#include "worker.hpp"

#include "ctrl/controller.hpp"
#include "ctrl/timer.hpp"
#include "dice/engine.hpp"
#include "dice/serializer.hpp"

#include "utils/log.hpp"

#include <cassert>
#include <chrono>

using namespace std::chrono_literals;

namespace core {
namespace {

void ScheduleOnMainWorker(std::function<void()> && task, std::chrono::milliseconds delay = 0ms)
{
   static auto onException = [](std::string_view worker, std::string_view exception) {
      Log::Error("MAIN", "Worker {} caught an exception: {}", worker, exception);
   };
   static async::Worker s_worker(async::Worker::Config{
      .name = "MAIN_WORKER",
      .capacity = std::numeric_limits<size_t>::max(),
      .exceptionHandler = onException,
   });
   s_worker.Schedule(delay, std::move(task));
}

IController & GetController()
{
   static auto s_ctrl = core::CreateController(dice::CreateUniformEngine(),
                                               std::make_unique<core::Timer>(ScheduleOnMainWorker),
                                               dice::CreateXmlSerializer());
   return *s_ctrl;
}

} // namespace

void InternalExec(std::function<void(IController *)> task)
{
   ScheduleOnMainWorker([task = std::move(task)] {
      task(&GetController());
   });
}

void Scheduler::await_suspend(stdcr::coroutine_handle<> h)
{
   core::Exec([h, this](core::IController * ctrl) mutable {
      m_ctrl = ctrl;
      h.resume();
   });
}

core::IController * Scheduler::await_resume() const noexcept
{
   assert(m_ctrl);
   return m_ctrl;
}

} // namespace core
