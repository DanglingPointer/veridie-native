#include "fsm/stateplaying.hpp"

#include "ctrl/timer.hpp"
#include "dice/engine.hpp"
#include "fsm/stateidle.hpp"
#include "fsm/statenegotiating.hpp"
#include "sign/commands.hpp"

#include "utils/log.hpp"

#include <functional>
#include <vector>

using namespace std::chrono_literals;
namespace {

constexpr auto TAG = "FSM";
constexpr uint32_t RETRY_COUNT = 5U;
constexpr uint32_t REQUEST_ATTEMPTS = 3U;
constexpr uint32_t ROUNDS_PER_GENERATOR = 10U;
constexpr auto IGNORE_OFFERS_DURATION = 10s;

bool Matches(const dice::Response & response, const dice::Request * request)
{
   if (!request)
      return false;

   if (response.cast.index() != request->cast.index())
      return false;

   auto getSize = [](const auto & vec) {
      return vec.size();
   };
   size_t responseSize = response.cast.Apply(getSize);
   size_t requestSize = request->cast.Apply(getSize);

   if (responseSize != requestSize)
      return false;

   return response.successCount.has_value() == request->threshold.has_value();
}

dice::Response GenerateResponse(dice::IEngine & engine, dice::Request && request)
{
   engine.GenerateResult(request.cast);
   std::optional<size_t> successCount;
   if (request.threshold)
      successCount = dice::GetSuccessCount(request.cast, *request.threshold);
   return dice::Response{std::move(request.cast), successCount};
}

} // namespace

namespace fsm {

// Handles connection errors, retries, reconnections(?), buffering etc
// We assume no new requests until the last one has been answered

StatePlaying::RemotePeerManager::RemotePeerManager(const bt::Device & remote,
                                                   core::CommandAdapter & proxy,
                                                   core::Timer & timer,
                                                   bool isGenerator,
                                                   std::function<void()> renegotiate)
   : m_remote(remote)
   , m_proxy(proxy)
   , m_timer(timer)
   , m_renegotiate(std::move(renegotiate))
   , m_isGenerator(isGenerator)
   , m_pendingRequest(false)
   , m_connected(true)
{}

StatePlaying::RemotePeerManager::~RemotePeerManager()
{
   if (!m_connected)
      m_proxy.FireAndForget<cmd::CloseConnection>("Connection has been lost", m_remote.mac);
}

const bt::Device & StatePlaying::RemotePeerManager::GetDevice() const
{
   return m_remote;
}

bool StatePlaying::RemotePeerManager::IsConnected() const
{
   return m_connected;
}

bool StatePlaying::RemotePeerManager::IsGenerator() const
{
   return m_isGenerator;
}

void StatePlaying::RemotePeerManager::SendRequest(const std::string & request)
{
   m_pendingRequest = true;
   StartTask(m_isGenerator ? SendRequestToGenerator(request) : Send(request));
}

void StatePlaying::RemotePeerManager::SendResponse(const std::string & response)
{
   StartTask(Send(response));
}

void StatePlaying::RemotePeerManager::OnReceptionSuccess(bool answeredRequest)
{
   m_connected = true;
   if (answeredRequest)
      m_pendingRequest = false;
}

void StatePlaying::RemotePeerManager::OnReceptionFailure()
{
   m_connected = false;
   if (m_isGenerator)
      m_renegotiate();
}

cr::TaskHandle<void> StatePlaying::RemotePeerManager::SendRequestToGenerator(std::string request)
{
   for (unsigned attempt = REQUEST_ATTEMPTS; attempt > 0; --attempt) {
      co_await StartNestedTask(Send(request));
      co_await m_timer.WaitFor(1s);
      if (!m_pendingRequest)
         co_return;
   }
   m_renegotiate();
}

cr::TaskHandle<void> StatePlaying::RemotePeerManager::Send(std::string message)
{
   if (message.size() > cmd::SendLongMessage::MAX_BUFFER_SIZE) {
      m_proxy.FireAndForget<cmd::ShowToast>("Cannot send too long message, try fewer dices", 7s);
      co_return;
   }

   unsigned retriesLeft = RETRY_COUNT;

   do {
      cmd::SendMessageResponse response;
      if (message.size() <= cmd::SendMessage::MAX_BUFFER_SIZE)
         response = co_await m_proxy.Command<cmd::SendMessage>(message, m_remote.mac);
      else
         response = co_await m_proxy.Command<cmd::SendLongMessage>(message, m_remote.mac);

      switch (response) {
      case cmd::SendMessageResponse::INVALID_STATE:
      case cmd::SendMessageResponse::INTEROP_FAILURE:
         break;
      case cmd::SendMessageResponse::OK:
         m_connected = true;
         if (m_queuedMessages.empty())
            co_return;

         message = std::move(m_queuedMessages.back());
         m_queuedMessages.pop_back();
         retriesLeft = RETRY_COUNT + 1;
         break;
      default:
         m_connected = false;
         m_queuedMessages.emplace_back(std::move(message));
         if (m_isGenerator)
            m_renegotiate();
         co_return;
      }
   } while (--retriesLeft > 0);
}


StatePlaying::StatePlaying(const Context & ctx,
                           std::unordered_set<bt::Device> && peers,
                           std::string && localMac,
                           std::string && generatorMac)
   : m_ctx(ctx)
   , m_localMac(std::move(localMac))
   , m_localGenerator(m_localMac == generatorMac)
   , m_ignoreOffers(m_ctx.timer->WaitFor(IGNORE_OFFERS_DURATION))
   , m_responseCount(0U)
{
   Log::Info(TAG, "New state: {}", __func__);
   m_ignoreOffers.Run(Executor());

   for (const auto & peer : peers) {
      bool isGenerator = !m_localGenerator && peer.mac == generatorMac;
      m_managers.emplace(std::piecewise_construct,
                         std::forward_as_tuple(peer.mac),
                         std::forward_as_tuple(peer,
                                               std::ref(m_ctx.proxy),
                                               std::ref(*m_ctx.timer),
                                               isGenerator,
                                               [this] {
                                                  StartNegotiation();
                                               }));
   }
}

StatePlaying::~StatePlaying() = default;

void StatePlaying::OnBluetoothOff()
{
   m_ctx.proxy.FireAndForget<cmd::ResetConnections>();
   m_ctx.proxy.FireAndForget<cmd::ResetGame>();
   Context::SwitchToState<StateIdle>(m_ctx);
}

void StatePlaying::OnDeviceConnected(const bt::Device & remote)
{
   // enable listening for this to be useful?
   if (auto mgr = m_managers.find(remote.mac); mgr != std::cend(m_managers))
      mgr->second.OnReceptionSuccess(true);
}

void StatePlaying::OnMessageReceived(const bt::Device & sender, const std::string & message)
{
   auto mgr = m_managers.find(sender.mac);
   if (mgr == std::cend(m_managers))
      return;

   try {
      auto parsed = m_ctx.serializer->Deserialize(message);

      if (std::holds_alternative<dice::Offer>(parsed)) {
         mgr->second.OnReceptionSuccess(m_pendingRequest == nullptr);
         if (!m_ignoreOffers)
            StartNegotiationWithOffer(sender, message);
         return;
      }

      if (auto * response = std::get_if<dice::Response>(&parsed)) {
         if (!mgr->second.IsGenerator())
            return;
         if (Matches(*response, m_pendingRequest.get()))
            m_pendingRequest = nullptr;
         mgr->second.OnReceptionSuccess(m_pendingRequest == nullptr);
         StartTask(ShowResponse(*response, mgr->second.GetDevice().name));
         return;
      }

      if (auto * request = std::get_if<dice::Request>(&parsed)) {
         mgr->second.OnReceptionSuccess(m_pendingRequest == nullptr);
         StartTask(ShowRequest(*request, mgr->second.GetDevice().name));
         if (m_localGenerator) {
            dice::Response response = GenerateResponse(*m_ctx.generator, std::move(*request));
            std::string encoded = m_ctx.serializer->Serialize(response);
            for (auto & [_, peer] : m_managers)
               peer.SendResponse(encoded);
            StartTask(ShowResponse(response, "You"));
         }
         return;
      }
   }
   catch (const std::exception & e) {
      Log::Error(TAG, "StateConnecting::{}(): {}", __func__, e.what());
   }
}

void StatePlaying::OnCastRequest(dice::Request && localRequest)
{
   StartTask(ShowRequest(localRequest, "You"));

   std::string encodedRequest = m_ctx.serializer->Serialize(localRequest);
   for (auto & [_, mgr] : m_managers)
      mgr.SendRequest(encodedRequest);

   if (m_localGenerator) {
      dice::Response response = GenerateResponse(*m_ctx.generator, std::move(localRequest));
      std::string encodedResponse = m_ctx.serializer->Serialize(response);
      for (auto & [_, mgr] : m_managers)
         mgr.SendResponse(encodedResponse);
      StartTask(ShowResponse(response, "You"));
   } else {
      m_pendingRequest = std::make_unique<dice::Request>(std::move(localRequest));
   }
}

void StatePlaying::OnGameStopped()
{
   m_ctx.proxy.FireAndForget<cmd::ResetConnections>();
   m_ctx.proxy.FireAndForget<cmd::ResetGame>();
   Context::SwitchToState<StateIdle>(m_ctx);
}

void StatePlaying::OnSocketReadFailure(const bt::Device & transmitter)
{
   if (auto mgr = m_managers.find(transmitter.mac); mgr != std::cend(m_managers))
      mgr->second.OnReceptionFailure();
}

void StatePlaying::StartNegotiation()
{
   std::unordered_set<bt::Device> peers;
   for (const auto & [_, mgr] : m_managers)
      if (mgr.IsConnected())
         peers.emplace(mgr.GetDevice());

   Context::SwitchToState<StateNegotiating>(m_ctx, std::move(peers), m_localMac);
}

void StatePlaying::StartNegotiationWithOffer(const bt::Device & sender, const std::string & offer)
{
   std::unordered_set<bt::Device> peers;
   for (const auto & [_, mgr] : m_managers)
      if (mgr.IsConnected())
         peers.emplace(mgr.GetDevice());
   m_managers.clear();

   Context::SwitchToState<StateNegotiating>(m_ctx, std::move(peers), m_localMac, sender, offer);
}

cr::TaskHandle<void> StatePlaying::ShowRequest(const dice::Request & request,
                                               const std::string & from)
{
   const auto response =
      co_await m_ctx.proxy.Command<cmd::ShowRequest>(dice::TypeToString(request.cast),
                                                     request.cast.Apply([](const auto & vec) {
                                                        return vec.size();
                                                     }),
                                                     request.threshold.value_or(0U),
                                                     from);

   if (response != cmd::ShowRequestResponse::OK)
      OnGameStopped();
}

cr::TaskHandle<void> StatePlaying::ShowResponse(const dice::Response & response,
                                                const std::string & from)
{
   const size_t responseSize = response.cast.Apply([](const auto & vec) {
      return vec.size();
   });

   if (responseSize > cmd::ShowLongResponse::MAX_BUFFER_SIZE / 3) {
      m_ctx.proxy.FireAndForget<cmd::ShowToast>("Request is too big, cannot proceed", 7s);
      co_return;
   }

   cmd::ShowResponseResponse responseCode;

   if (responseSize <= cmd::ShowResponse::MAX_BUFFER_SIZE / 3) {
      responseCode =
         co_await m_ctx.proxy.Command<cmd::ShowResponse>(response.cast,
                                                         dice::TypeToString(response.cast),
                                                         response.successCount.value_or(-1),
                                                         from);
   } else {
      responseCode =
         co_await m_ctx.proxy.Command<cmd::ShowLongResponse>(response.cast,
                                                             dice::TypeToString(response.cast),
                                                             response.successCount.value_or(-1),
                                                             from);
   }

   if (responseCode != cmd::ShowResponseResponse::OK)
      OnGameStopped();
   else if (++m_responseCount >= ROUNDS_PER_GENERATOR)
      StartNegotiation();
}

} // namespace fsm
