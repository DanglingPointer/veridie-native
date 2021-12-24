#include "sign/commandmanager.hpp"
#include "sign/externalinvoker.hpp"

#include "utils/log.hpp"

#undef NDEBUG
#include <cassert>

namespace cmd {
namespace {
constexpr int32_t INVALID_CMD_ID = 0;
constexpr auto TAG = "Command";
} // namespace


Manager::FutureResponse::FutureResponse(Manager & mgr, int32_t id) noexcept
   : m_mgr(mgr)
   , m_id(id)
{}

bool Manager::FutureResponse::await_ready() const noexcept
{
   return m_id == INVALID_CMD_ID;
}

void Manager::FutureResponse::await_suspend(stdcr::coroutine_handle<> h) const
{
   assert(m_mgr.m_pendingCmds.count(m_id) != 0);
   m_mgr.m_pendingCmds[m_id].callback = h;
}

int64_t Manager::FutureResponse::await_resume() const
{
   const auto it = m_mgr.m_pendingCmds.find(m_id);
   if (it == std::end(m_mgr.m_pendingCmds))
      return ICommand::INTEROP_FAILURE;

   const int64_t response = it->second.response;
   m_mgr.m_pendingCmds.erase(it);
   return response;
}

Manager::Manager(std::unique_ptr<IExternalInvoker> uiInvoker,
                 std::unique_ptr<IExternalInvoker> btInvoker)
   : m_uiInvoker(std::move(uiInvoker))
   , m_btInvoker(std::move(btInvoker))
{
   assert(m_uiInvoker);
   assert(m_btInvoker);
}

Manager::~Manager()
{
   for (auto it = m_pendingCmds.begin(); it != std::end(m_pendingCmds);
        it = m_pendingCmds.begin()) {
      if (it->second.callback)
         it->second.callback();
      else
         m_pendingCmds.erase(it);
   }
}

Manager::FutureResponse Manager::IssueUiCommand(mem::pool_ptr<ICommand> && cmd)
{
   return IssueCommand(std::move(cmd), *m_uiInvoker);
}

Manager::FutureResponse Manager::IssueBtCommand(mem::pool_ptr<ICommand> && cmd)
{
   return IssueCommand(std::move(cmd), *m_btInvoker);
}

Manager::FutureResponse Manager::IssueCommand(mem::pool_ptr<ICommand> && cmd,
                                              IExternalInvoker & invoker)
{
   int32_t id = cmd->GetId();
   while (m_pendingCmds.count(id))
      ++id;

   if ((id - cmd->GetId()) >= COMMAND_ID(1)) {
      Log::Error(TAG, "Command storage is full for {}", cmd->GetName());
      return FutureResponse(*this, INVALID_CMD_ID);
   }

   if (!invoker.Invoke(std::move(cmd), id)) {
      Log::Error(TAG, "External Invoker failed");
      return FutureResponse(*this, INVALID_CMD_ID);
   }

   m_pendingCmds[id] = {};
   return FutureResponse(*this, id);
}

void Manager::SubmitResponse(int32_t cmdId, int64_t response)
{
   const auto it = m_pendingCmds.find(cmdId);
   if (it == std::end(m_pendingCmds)) {
      Log::Warning(TAG, "cmd::Manager received response to a non-existing command, ID = {}", cmdId);
      return;
   }
   if (!it->second.callback) {
      m_pendingCmds.erase(it);
      Log::Info(TAG, "cmd::Manager received an orphaned response, ID = {}", cmdId);
      return;
   }
   it->second.response = response;
   it->second.callback();
}

} // namespace cmd
