#ifndef FSM_STATENEGOTIATING_HPP
#define FSM_STATENEGOTIATING_HPP

#include "bt/device.hpp"
#include "dice/serializer.hpp"
#include "fsm/context.hpp"
#include "fsm/statebase.hpp"

#include "utils/task.hpp"

#include <map>
#include <string>
#include <unordered_set>

namespace fsm {

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
   ~StateNegotiating() override;
   void OnBluetoothOff() override;
   void OnMessageReceived(const bt::Device & sender, const std::string & message) override;
   void OnGameStopped() override;
   void OnSocketReadFailure(const bt::Device & from) override;

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

} // namespace fsm

#endif
