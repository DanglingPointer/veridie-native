#include "fsm/stateidle.hpp"

#include "fsm/stateconnecting.hpp"
#include "ctrl/timer.hpp"
#include "sign/commands.hpp"

#include "utils/log.hpp"

#include <chrono>

namespace fsm {
using namespace std::chrono_literals;

namespace {
constexpr auto TAG = "FSM";
}

StateIdle::StateIdle(const Context & ctx, bool startNewGame)
   : m_ctx(ctx)
   , m_newGamePending(false)
   , m_bluetoothOn(false)
{
   Log::Info(TAG, "New state: {}", __func__);
   m_enableBtTask = RequestBluetoothOn();
   m_enableBtTask.Run(Executor());
   cmd::pool.ShrinkToFit();

   if (startNewGame)
      OnNewGame();
}

void StateIdle::OnBluetoothOn()
{
   m_bluetoothOn = true;
   if (m_enableBtTask)
      m_enableBtTask = {};
   if (m_newGamePending) {
      Context::SwitchToState<StateConnecting>(m_ctx);
   }
}

void StateIdle::OnBluetoothOff()
{
   m_bluetoothOn = false;
   if (!m_enableBtTask) {
      m_enableBtTask = RequestBluetoothOn();
      m_enableBtTask.Run(Executor());
   }
}

void StateIdle::OnNewGame()
{
   m_newGamePending = true;
   if (m_bluetoothOn) {
      Context::SwitchToState<StateConnecting>(m_ctx);
   } else if (!m_enableBtTask) {
      m_enableBtTask = RequestBluetoothOn();
      m_enableBtTask.Run(Executor());
   }
}

cr::TaskHandle<void> StateIdle::RequestBluetoothOn()
{
   using Response = cmd::EnableBluetoothResponse;

   while (!m_bluetoothOn) {
      const Response result = co_await m_ctx.proxy.Command<cmd::EnableBluetooth>();

      switch (result) {
      case Response::INTEROP_FAILURE:
      case Response::INVALID_STATE:
         co_await m_ctx.timer->WaitFor(1s);
         break;
      case Response::OK:
         if (m_newGamePending)
            Context::SwitchToState<StateConnecting>(m_ctx);
         m_bluetoothOn = true;
         break;
      case Response::NO_BT_ADAPTER:
         m_ctx.proxy.FireAndForget<cmd::ShowAndExit>("Cannot proceed due to a fatal failure.");
         Context::SwitchToState<void>(m_ctx);
         [[fallthrough]];
      case Response::USER_DECLINED:
         co_return;
      }
   }
}

} // namespace fsm
