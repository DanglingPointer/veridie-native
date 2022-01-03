#include "fsm/statenegotiating.hpp"

#include "ctrl/timer.hpp"
#include "fsm/stateidle.hpp"
#include "fsm/stateplaying.hpp"
#include "sign/commands.hpp"

#include "utils/log.hpp"

#include <algorithm>
#include <iterator>

namespace fsm {
using namespace std::chrono_literals;

namespace {
constexpr auto TAG = "FSM";
}

uint32_t g_negotiationRound = 0U;

StateNegotiating::StateNegotiating(const fsm::Context & ctx,
                                   std::unordered_set<bt::Device> && peers,
                                   std::string && localMac)
   : m_ctx(ctx)
   , m_localMac(std::move(localMac))
   , m_peers(std::move(peers))
{
   Log::Info(TAG, "New state: {}", __func__);
   std::transform(std::cbegin(m_peers),
                  std::cend(m_peers),
                  std::inserter(m_offers, m_offers.end()),
                  [](const bt::Device & device) {
                     return std::make_pair(device.mac, dice::Offer{"", 0U});
                  });
   auto [localOffer, _] =
      m_offers.insert_or_assign(m_localMac, dice::Offer{"", ++g_negotiationRound});
   localOffer->second.mac = GetLocalOfferMac();

   StartRootTask(StartNegotiation());
}

StateNegotiating::StateNegotiating(const Context & ctx,
                                   std::unordered_set<bt::Device> && peers,
                                   std::string && localMac,
                                   const bt::Device & sender,
                                   const std::string & message)
   : StateNegotiating(ctx, std::move(peers), std::move(localMac))
{
   OnMessageReceived(sender, message);
}

StateNegotiating::~StateNegotiating() = default;

void StateNegotiating::OnBluetoothOff()
{
   m_ctx.proxy.FireAndForget<cmd::ResetConnections>();
   m_ctx.proxy.FireAndForget<cmd::ResetGame>();
   Context::SwitchToState<StateIdle>(m_ctx);
}

void StateNegotiating::OnMessageReceived(const bt::Device & sender, const std::string & message)
{
   if (m_peers.count(sender) == 0)
      return;

   try {
      auto decoded = m_ctx.serializer->Deserialize(message);
      auto & offer = std::get<dice::Offer>(decoded);
      m_offers[sender.mac] = std::move(offer);
   }
   catch (const std::exception & e) {
      Log::Error(TAG, "StateNegotiating::{}(): {}", __func__, e.what());
   }
}

void StateNegotiating::OnGameStopped()
{
   m_ctx.proxy.FireAndForget<cmd::ResetConnections>();
   Context::SwitchToState<StateIdle>(m_ctx);
}

void StateNegotiating::OnSocketReadFailure(const bt::Device & from)
{
   if (m_peers.count(from)) {
      StartRootTask(DisconnectDevice(from.mac));
      m_peers.erase(from);
      m_offers.erase(from.mac);
   }
}

const std::string & StateNegotiating::GetLocalOfferMac()
{
   size_t index = g_negotiationRound % m_offers.size();
   auto it = std::next(m_offers.cbegin(), index);
   return it->first;
}

cr::TaskHandle<void> StateNegotiating::StartNegotiation()
{
   using Response = cmd::NegotiationStartResponse;

   const Response response = co_await m_ctx.proxy.Command<cmd::NegotiationStart>();

   switch (response) {
   case Response::OK:
      co_await UpdateAndBroadcastOffer();
      break;
   case Response::INTEROP_FAILURE:
   case Response::INVALID_STATE:
      Log::Error(TAG, "{}: Cannot start negotiation in invalid state", __func__);
      break;
   }
}

cr::TaskHandle<void> StateNegotiating::UpdateAndBroadcastOffer()
{
   for (;;) {
      auto localOffer = m_offers.find(m_localMac);

      bool allOffersEqual =
         std::all_of(cbegin(m_offers), std::cend(m_offers), [localOffer](const auto & e) {
            return e.second.round == localOffer->second.round &&
                   e.second.mac == localOffer->second.mac;
         });

      if (allOffersEqual) {
         std::string_view nomineeName("You");
         if (auto it = m_peers.find(bt::Device{"", localOffer->second.mac});
             it != std::cend(m_peers)) {
            nomineeName = it->name;
         }
         m_ctx.proxy.FireAndForget<cmd::NegotiationStop>(nomineeName);
         Context::SwitchToState<StatePlaying>(m_ctx,
                                              std::move(m_peers),
                                              std::move(m_localMac),
                                              std::move(localOffer->second.mac));
         co_return;
      }

      uint32_t maxRound = g_negotiationRound;
      for (const auto & [_, offer] : m_offers) {
         if (offer.round > maxRound)
            maxRound = offer.round;
      }

      // update local offer
      if (maxRound > g_negotiationRound)
         g_negotiationRound = maxRound;
      localOffer->second.round = g_negotiationRound;
      localOffer->second.mac = GetLocalOfferMac();

      // broadcast local offer
      std::string message = m_ctx.serializer->Serialize(localOffer->second);
      for (const auto & remote : m_peers) {
         co_await StartNestedTask(SendOffer(message, remote));
      }
      co_await m_ctx.timer->WaitFor(1s);
   }
}

cr::TaskHandle<void> StateNegotiating::SendOffer(std::string offer, bt::Device receiver)
{
   using Response = cmd::SendMessageResponse;

   const Response response = co_await m_ctx.proxy.Command<cmd::SendMessage>(offer, receiver.mac);

   switch (response) {
   case Response::SOCKET_ERROR:
      co_await DisconnectDevice(receiver.mac);
      [[fallthrough]];
   case Response::CONNECTION_NOT_FOUND:
      m_peers.erase(receiver);
      m_offers.erase(receiver.mac);
      break;
   default:
      break;
   }
}

cr::TaskHandle<void> StateNegotiating::DisconnectDevice(std::string mac)
{
   using Response = cmd::CloseConnectionResponse;

   Response response;

   do {
      response = co_await m_ctx.proxy.Command<cmd::CloseConnection>("", mac);
   } while (response == Response::INVALID_STATE || response == Response::INTEROP_FAILURE);
}

} // namespace fsm
