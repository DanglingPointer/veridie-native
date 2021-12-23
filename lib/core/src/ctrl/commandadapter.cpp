#include "ctrl/commandadapter.hpp"
#include "sign/commandmanager.hpp"

#include "utils/log.hpp"

#include <sstream>

namespace core {
namespace {

constexpr auto TAG = "Command";

void LogCommand(const mem::pool_ptr<cmd::ICommand> & cmd)
{
   std::ostringstream ss;
   for (size_t i = 0; i < cmd->GetArgsCount(); ++i)
      ss << " [" << cmd->GetArgAt(i) << "]";
   Log::Info(TAG, ">>>>> {}{}", cmd->GetName(), ss.str());
}

void LogResponse(std::string_view cmdName, int64_t response)
{
   Log::Info(TAG,
             "<<<<< {}Response [{}]",
             cmdName,
             cmd::ToString(static_cast<cmd::ICommand::ResponseCode>(response)));
}

} // namespace

cr::TaskHandle<int64_t> CommandAdapter::ForwardUiCommand(mem::pool_ptr<cmd::ICommand> && cmd)
{
   LogCommand(cmd);
   const std::string_view name = cmd->GetName();
   const int64_t response = co_await m_manager.IssueUiCommand(std::move(cmd));
   LogResponse(name, response);
   co_return response;
}

cr::TaskHandle<int64_t> CommandAdapter::ForwardBtCommand(mem::pool_ptr<cmd::ICommand> && cmd)
{
   LogCommand(cmd);
   const std::string_view name = cmd->GetName();
   const int64_t response = co_await m_manager.IssueBtCommand(std::move(cmd));
   LogResponse(name, response);
   co_return response;
}

void CommandAdapter::DetachedUiCommand(mem::pool_ptr<cmd::ICommand> && cmd)
{
   LogCommand(cmd);
   m_manager.IssueUiCommand(std::move(cmd));
}

void CommandAdapter::DetachedBtCommand(mem::pool_ptr<cmd::ICommand> && cmd)
{
   LogCommand(cmd);
   m_manager.IssueBtCommand(std::move(cmd));
}

} // namespace core
