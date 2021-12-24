#ifndef FSM_STATES_HPP
#define FSM_STATES_HPP

#include "fsm/context.hpp"
#include "fsm/statebase.hpp"
#include "ctrl/timer.hpp"

#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace bt {
struct Device;
}
namespace dice {
struct Offer;
struct Request;
struct Response;
} // namespace dice

namespace fsm {

class StateIdle : public StateBase
{
public:
   explicit StateIdle(const Context & ctx, bool startNewGame = false);
   void OnBluetoothOn();
   void OnBluetoothOff();
   void OnNewGame();

private:
   [[nodiscard]] cr::TaskHandle<void> RequestBluetoothOn();

   Context m_ctx;
   cr::TaskHandle<void> m_enableBtTask;

   bool m_newGamePending;
   bool m_bluetoothOn;
};

class StateConnecting : public StateBase
{
public:
   explicit StateConnecting(const Context & ctx);
   ~StateConnecting();
   void OnBluetoothOff();
   void OnDeviceConnected(const bt::Device & remote);
   void OnDeviceDisconnected(const bt::Device & remote);
   void OnMessageReceived(const bt::Device & sender, const std::string & message);
   void OnConnectivityEstablished();
   void OnGameStopped();
   void OnSocketReadFailure(const bt::Device & from);

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

class StateNegotiating : public StateBase
{
public:
   StateNegotiating(const Context & ctx,
                    std::unordered_set<bt::Device> && peers,
                    std::string && localMac);
   StateNegotiating(const Context & ctx,
                    std::unordered_set<bt::Device> && peers,
                    std::string && localMac,
                    const bt::Device & sender,
                    const std::string & message);
   ~StateNegotiating();
   void OnBluetoothOff();
   void OnMessageReceived(const bt::Device & sender, const std::string & message);
   void OnGameStopped();
   void OnSocketReadFailure(const bt::Device & from);

private:
   [[nodiscard]] cr::TaskHandle<void> StartNegotiation();
   [[nodiscard]] cr::TaskHandle<void> UpdateAndBroadcastOffer();
   [[nodiscard]] cr::TaskHandle<void> SendOffer(std::string offer, bt::Device receiver);
   const std::string & GetLocalOfferMac();
   [[nodiscard]] cr::TaskHandle<void> DisconnectDevice(std::string mac);

   Context m_ctx;
   std::string m_localMac;

   std::unordered_set<bt::Device> m_peers;
   std::map<std::string, dice::Offer> m_offers;
};

class StatePlaying : public StateBase
{
public:
   StatePlaying(const Context & ctx,
                std::unordered_set<bt::Device> && peers,
                std::string && localMac,
                std::string && generatorMac);
   ~StatePlaying();
   void OnBluetoothOff();
   void OnDeviceConnected(const bt::Device & remote);
   void OnMessageReceived(const bt::Device & sender, const std::string & message);
   void OnCastRequest(dice::Request && localRequest);
   void OnGameStopped();
   void OnSocketReadFailure(const bt::Device & transmitter);

private:
   class RemotePeerManager;

   void StartNegotiation();
   void StartNegotiationWithOffer(const bt::Device & sender, const std::string & offer);
   [[nodiscard]] cr::TaskHandle<void> ShowRequest(const dice::Request & request,
                                                  const std::string & from);
   [[nodiscard]] cr::TaskHandle<void> ShowResponse(const dice::Response & response,
                                                   const std::string & from);

   Context m_ctx;
   const std::string m_localMac;
   bool m_localGenerator;

   cr::TaskHandle<core::Timeout> m_ignoreOffers;
   std::unique_ptr<dice::Request> m_pendingRequest;

   std::unordered_map<std::string, RemotePeerManager> m_managers;
   uint32_t m_responseCount;
};

} // namespace fsm

#endif // FSM_STATES_HPP
