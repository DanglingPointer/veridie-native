#ifndef CORE_EXEC_HPP
#define CORE_EXEC_HPP

#include "utils/alwayscopyable.hpp"
#include "utils/coroutine.hpp"

#include <functional>

namespace core {
class IController;

void InternalExec(std::function<void(IController *)> task);

template <typename F>
void Exec(F && f)
{
   InternalExec(AlwaysCopyable(std::move(f)));
}

struct Scheduler
{
   bool await_ready() const noexcept { return false; }
   void await_suspend(stdcr::coroutine_handle<> h);
   core::IController * await_resume() const noexcept;

private:
   core::IController * m_ctrl = nullptr;
};

} // namespace core

#endif // CORE_EXEC_HPP
