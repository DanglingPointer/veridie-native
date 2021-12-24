#ifndef FSM_STATECONNECTING_HPP
#define FSM_STATECONNECTING_HPP

#include "bt/device.hpp"
#include "fsm/context.hpp"
#include "fsm/statebase.hpp"

#include "utils/task.hpp"

#include <optional>
#include <string>
#include <unordered_set>

namespace fsm {

class StateConnecting : public StateBase
{
public:
   explicit StateConnecting(const Context & ctx);
   ~StateConnecting() override;
   void OnBluetoothOff() override;
   void OnDeviceConnected(const bt::Device & remote) override;
   void OnDeviceDisconnected(const bt::Device & remote) override;
   void OnMessageReceived(const bt::Device & sender, const std::string & message) override;
   void OnConnectivityEstablished() override;
   void OnGameStopped() override;
   void OnSocketReadFailure(const bt::Device & from) override;

private:
   void DetectFatalFailure();
   [[nodiscard]] cr::TaskHandle<void> SendHelloTo(std::string mac);
   [[nodiscard]] cr::TaskHandle<void> DisconnectDevice(std::string mac);
   [[nodiscard]] cr::TaskHandle<void> AttemptNegotiationStart();
   [[nodiscard]] cr::TaskHandle<void> KickOffDiscovery();
   [[nodiscard]] cr::TaskHandle<void> KickOffListening();

   Context m_ctx;

   std::optional<bool> m_discovering;
   std::optional<bool> m_listening;

   std::optional<std::string> m_localMac;
   std::unordered_set<bt::Device> m_peers;
   cr::TaskHandle<void> m_retryStartHandle;
};

} // namespace fsm

#endif
