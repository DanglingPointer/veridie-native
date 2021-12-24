#ifndef FSM_STATEPLAYING_HPP
#define FSM_STATEPLAYING_HPP

#include "bt/device.hpp"
#include "dice/serializer.hpp"
#include "fsm/context.hpp"
#include "fsm/statebase.hpp"

#include "utils/task.hpp"
#include "utils/taskowner.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fsm {

class StatePlaying : public StateBase
{
   class RemotePeerManager : private cr::TaskOwner<>
   {
   public:
      RemotePeerManager(const bt::Device & remote,
                        core::CommandAdapter & proxy,
                        core::Timer & timer,
                        bool isGenerator,
                        std::function<void()> renegotiate);
      ~RemotePeerManager();
      const bt::Device & GetDevice() const;
      bool IsConnected() const;
      bool IsGenerator() const;
      void SendRequest(const std::string & request);
      void SendResponse(const std::string & response);
      void OnReceptionSuccess(bool answeredRequest);
      void OnReceptionFailure();

   private:
      [[nodiscard]] cr::TaskHandle<void> SendRequestToGenerator(std::string request);
      [[nodiscard]] cr::TaskHandle<void> Send(std::string message);

      bt::Device m_remote;
      core::CommandAdapter & m_proxy;
      core::Timer & m_timer;
      std::function<void()> m_renegotiate;
      const bool m_isGenerator;

      bool m_pendingRequest;
      bool m_connected;
      std::vector<std::string> m_queuedMessages;
   };

public:
   StatePlaying(const Context & ctx,
                std::unordered_set<bt::Device> && peers,
                std::string && localMac,
                std::string && generatorMac);
   ~StatePlaying() override;
   void OnBluetoothOff() override;
   void OnDeviceConnected(const bt::Device & remote) override;
   void OnMessageReceived(const bt::Device & sender, const std::string & message) override;
   void OnCastRequest(dice::Request && localRequest) override;
   void OnGameStopped() override;
   void OnSocketReadFailure(const bt::Device & transmitter) override;

private:
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

#endif
