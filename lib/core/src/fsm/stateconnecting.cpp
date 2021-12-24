#include "fsm/stateconnecting.hpp"

#include "fsm/stateidle.hpp"
#include "fsm/statenegotiating.hpp"
#include "ctrl/timer.hpp"
#include "dice/serializer.hpp"
#include "sign/commands.hpp"

#include "utils/log.hpp"

#include <chrono>

using namespace std::chrono_literals;

namespace {

constexpr auto TAG = "FSM";
constexpr auto APP_UUID = "76445157-4f39-42e9-a62e-877390cbb4bb";
constexpr auto APP_NAME = "VeriDie";
constexpr uint32_t MAX_SEND_RETRY_COUNT = 10U;
constexpr uint32_t MAX_GAME_START_RETRY_COUNT = 30U;
constexpr uint32_t MAX_DISCOVERY_RETRY_COUNT = 2U;
constexpr uint32_t MAX_LISTENING_RETRY_COUNT = 2U;
constexpr auto DISCOVERABILITY_DURATION = 5min;

} // namespace


namespace fsm {

StateConnecting::StateConnecting(const Context & ctx)
   : m_ctx(ctx)
{
   Log::Info(TAG, "New state: {}", __func__);
   StartTask(KickOffDiscovery());
   StartTask(KickOffListening());
}

StateConnecting::~StateConnecting()
{
   if (m_discovering.value_or(false))
      m_ctx.proxy.FireAndForget<cmd::StopDiscovery>();
   if (m_listening.value_or(false))
      m_ctx.proxy.FireAndForget<cmd::StopListening>();
}

void StateConnecting::OnBluetoothOff()
{
   Context::SwitchToState<StateIdle>(m_ctx, true);
}

void StateConnecting::OnDeviceConnected(const bt::Device & remote)
{
   m_peers.insert(remote);
   StartTask(SendHelloTo(remote.mac));
}

void StateConnecting::OnDeviceDisconnected(const bt::Device & remote)
{
   m_peers.erase(remote);
}

void StateConnecting::OnMessageReceived(const bt::Device & sender, const std::string & message)
{
   if (m_peers.count(sender) == 0)
      OnDeviceConnected(sender);

   if (m_localMac.has_value())
      return;

   try {
      auto decoded = m_ctx.serializer->Deserialize(message);
      auto & hello = std::get<dice::Hello>(decoded);
      m_localMac = std::move(hello.mac);
   }
   catch (const std::exception & e) {
      Log::Error(TAG, "StateConnecting::{}(): {}", __func__, e.what());
   }
}

void StateConnecting::OnSocketReadFailure(const bt::Device & from)
{
   if (m_peers.count(from)) {
      m_peers.erase(from);
      StartTask(DisconnectDevice(from.mac));
   }
}

void StateConnecting::OnConnectivityEstablished()
{
   if (m_retryStartHandle)
      return;
   m_retryStartHandle = AttemptNegotiationStart();
   m_retryStartHandle.Run(Executor());
}

void StateConnecting::OnGameStopped()
{
   m_ctx.proxy.FireAndForget<cmd::ResetConnections>();
   Context::SwitchToState<StateIdle>(m_ctx);
}

void StateConnecting::DetectFatalFailure()
{
   if (m_listening.has_value() && !*m_listening && m_discovering.has_value() && !*m_discovering) {
      m_ctx.proxy.FireAndForget<cmd::ShowAndExit>("Cannot proceed due to a fatal failure.");
      Context::SwitchToState<void>(m_ctx);
   }
}

cr::TaskHandle<void> StateConnecting::SendHelloTo(std::string mac)
{
   using Response = cmd::SendMessageResponse;

   int retriesLeft = MAX_SEND_RETRY_COUNT;
   const std::string hello = m_ctx.serializer->Serialize(dice::Hello{mac});
   Response response;

   do {
      if (m_peers.count(bt::Device{"", mac}) == 0)
         co_return;

      response = co_await m_ctx.proxy.Command<cmd::SendMessage>(hello, mac);

      if (response == Response::CONNECTION_NOT_FOUND)
         OnDeviceDisconnected(bt::Device{"", mac});
      else if (response == Response::SOCKET_ERROR) {
         m_peers.erase(bt::Device{"", mac});
         co_await StartNestedTask(DisconnectDevice(mac));
      }

   } while (--retriesLeft > 0 && response == Response::INVALID_STATE);
}

cr::TaskHandle<void> StateConnecting::DisconnectDevice(std::string mac)
{
   using Response = cmd::CloseConnectionResponse;
   Response response;

   do {
      response = co_await m_ctx.proxy.Command<cmd::CloseConnection>("", mac);
   } while (response == Response::INVALID_STATE);
}

cr::TaskHandle<void> StateConnecting::AttemptNegotiationStart()
{
   unsigned retriesLeft = MAX_GAME_START_RETRY_COUNT;

   do {
      if (m_localMac.has_value()) {
         cmd::pool.Resize(m_peers.size());
         Context::SwitchToState<StateNegotiating>(m_ctx,
                                                  std::move(m_peers),
                                                  *std::move(m_localMac));
         co_return;
      }

      if (retriesLeft % 3 == 0)
         m_ctx.proxy.FireAndForget<cmd::ShowToast>("Getting ready...", 3s);

      co_await m_ctx.timer->WaitFor(1s);

   } while (--retriesLeft > 0);

   m_ctx.proxy.FireAndForget<cmd::ResetGame>();
   m_ctx.proxy.FireAndForget<cmd::ResetConnections>();
   Context::SwitchToState<StateIdle>(m_ctx);
}

cr::TaskHandle<void> StateConnecting::KickOffDiscovery()
{
   using Response = cmd::StartDiscoveryResponse;

   unsigned retriesLeft = MAX_DISCOVERY_RETRY_COUNT;
   Response response;

   do {
      const bool includePaired = true;
      response =
         co_await m_ctx.proxy.Command<cmd::StartDiscovery>(APP_UUID, APP_NAME, includePaired);

      switch (response) {
      case Response::OK:
         m_discovering = true;
         break;
      case Response::BLUETOOTH_OFF:
         OnBluetoothOff();
         break;
      case Response::INVALID_STATE:
         co_await m_ctx.timer->WaitFor(1s);
         break;
      default:
         m_discovering = false;
         break;
      }

   } while (retriesLeft-- > 0 && response == Response::INVALID_STATE);

   if (response == Response::INVALID_STATE) {
      m_discovering = false;
      DetectFatalFailure();
   }
}

cr::TaskHandle<void> StateConnecting::KickOffListening()
{
   using Response = cmd::StartListeningResponse;

   unsigned retriesLeft = MAX_LISTENING_RETRY_COUNT;
   Response response;

   do {
      response = co_await m_ctx.proxy.Command<cmd::StartListening>(APP_UUID,
                                                                   APP_NAME,
                                                                   DISCOVERABILITY_DURATION);

      switch (response) {
      case Response::OK:
         m_listening = true;
         co_return;
      case Response::BLUETOOTH_OFF:
         OnBluetoothOff();
         co_return;
      case Response::USER_DECLINED:
         m_listening = false;
         DetectFatalFailure();
         co_return;
      default:
         co_await m_ctx.timer->WaitFor(1s);
         break;
      }

   } while (retriesLeft-- > 0);

   m_listening = false;
   DetectFatalFailure();
}

} // namespace fsm
