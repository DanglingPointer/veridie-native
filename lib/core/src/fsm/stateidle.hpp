#ifndef FSM_STATEIDLE_HPP
#define FSM_STATEIDLE_HPP

#include "fsm/context.hpp"
#include "fsm/statebase.hpp"

#include "utils/task.hpp"

namespace fsm {

class StateIdle : public StateBase
{
public:
   explicit StateIdle(const Context & ctx, bool startNewGame = false);
   void OnBluetoothOn() override;
   void OnBluetoothOff() override;
   void OnNewGame() override;

private:
   [[nodiscard]] cr::TaskHandle<void> RequestBluetoothOn();

   Context m_ctx;
   cr::TaskHandle<void> m_enableBtTask;

   bool m_newGamePending;
   bool m_bluetoothOn;
};

} // namespace fsm

#endif
