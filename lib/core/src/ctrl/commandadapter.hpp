#ifndef CORE_COMMANDADAPTER_HPP
#define CORE_COMMANDADAPTER_HPP

#include "utils/task.hpp"
#include "utils/poolptr.hpp"
#include "sign/commandpool.hpp"

namespace cmd {
class Manager;
}

namespace core {

class CommandAdapter
{
public:
   CommandAdapter(cmd::Manager & manager)
      : m_manager(manager)
   {}

   template <cmd::UiCommand TCmd, typename... TArgs>
   cr::TaskHandle<typename TCmd::Response> Command(TArgs &&... args)
   {
      auto pcmd = cmd::pool.MakeUnique<TCmd>(std::forward<TArgs>(args)...);
      const int64_t response = co_await ForwardUiCommand(std::move(pcmd));
      co_return static_cast<typename TCmd::Response>(response);
   }

   template <cmd::BtCommand TCmd, typename... TArgs>
   cr::TaskHandle<typename TCmd::Response> Command(TArgs &&... args)
   {
      auto pcmd = cmd::pool.MakeUnique<TCmd>(std::forward<TArgs>(args)...);
      const int64_t response = co_await ForwardBtCommand(std::move(pcmd));
      co_return static_cast<typename TCmd::Response>(response);
   }

   template <cmd::UiCommand TCmd, typename... TArgs>
   void FireAndForget(TArgs &&... args)
   {
      auto pcmd = cmd::pool.MakeUnique<TCmd>(std::forward<TArgs>(args)...);
      DetachedUiCommand(std::move(pcmd));
   }

   template <cmd::BtCommand TCmd, typename... TArgs>
   void FireAndForget(TArgs &&... args)
   {
      auto pcmd = cmd::pool.MakeUnique<TCmd>(std::forward<TArgs>(args)...);
      DetachedBtCommand(std::move(pcmd));
   }

private:
   cr::TaskHandle<int64_t> ForwardUiCommand(mem::pool_ptr<cmd::ICommand> && cmd);
   cr::TaskHandle<int64_t> ForwardBtCommand(mem::pool_ptr<cmd::ICommand> && cmd);
   void DetachedUiCommand(mem::pool_ptr<cmd::ICommand> && cmd);
   void DetachedBtCommand(mem::pool_ptr<cmd::ICommand> && cmd);

   cmd::Manager & m_manager;
};

} // namespace core

#endif
