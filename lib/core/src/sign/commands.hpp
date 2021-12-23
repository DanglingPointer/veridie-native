#ifndef SIGN_COMMANDS_HPP
#define SIGN_COMMANDS_HPP

#include "sign/cmd.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include "dice/cast.hpp"

namespace cmd {

template <typename TTraits>
class Base : public ICommand
{
   static_assert(TTraits::ID >= (100 << 8));
   static_assert(TTraits::LONG_BUFFER_SIZE >= TTraits::SHORT_BUFFER_SIZE);
   using LongBuffer = std::array<char, TTraits::LONG_BUFFER_SIZE>;
   using ShortBuffer = std::array<char, TTraits::SHORT_BUFFER_SIZE>;

public:
   using Response = typename TTraits::Response;
   using ParamTuple = typename TTraits::ParamTuple;

   static constexpr int32_t ID = TTraits::ID;
   static constexpr size_t ARG_SIZE = std::tuple_size_v<ParamTuple>;
   static constexpr size_t MAX_BUFFER_SIZE = TTraits::LONG_BUFFER_SIZE - 1;


   template <typename... Ts>
   explicit Base(Ts &&... params)
      : Base(ParamTuple(std::forward<Ts>(params)...))
   {}

   explicit Base(ParamTuple params);
   int32_t GetId() const noexcept override;
   std::string_view GetName() const noexcept override;
   size_t GetArgsCount() const noexcept override;
   std::string_view GetArgAt(size_t index) const noexcept override;

private:
   std::array<LongBuffer, (ARG_SIZE != 0)> m_longArgs{};
   std::array<ShortBuffer, (ARG_SIZE > 1) ? (ARG_SIZE - 1) : 0U> m_shortArgs{};
};


template <int32_t Id, typename TResponse, typename... TParams>
struct Traits
{
   static_assert(sizeof(TResponse) >= sizeof(int64_t));

   static constexpr int32_t ID = Id;
   using Command = Base<Traits<Id, TResponse, TParams...>>;
   using Response = TResponse;
   using ParamTuple = std::tuple<
      std::conditional_t<(!std::is_trivially_copyable_v<TParams> || sizeof(TParams) > 16U),
                         const TParams &,
                         TParams>...>;

   static constexpr size_t LONG_BUFFER_SIZE = 32U;
   static constexpr size_t SHORT_BUFFER_SIZE = 24U;
};

template <int32_t Id, typename TResponse, typename... TParams>
struct LongTraits : Traits<Id, TResponse, TParams...>
{
   static constexpr size_t LONG_BUFFER_SIZE = 256U;
   static constexpr size_t SHORT_BUFFER_SIZE = 32U;
};

template <int32_t Id, typename TResponse, typename... TParams>
struct ExtraLongTraits : Traits<Id, TResponse, TParams...>
{
   static constexpr size_t LONG_BUFFER_SIZE = 1024U;
   static constexpr size_t SHORT_BUFFER_SIZE = 32U;
};


// clang-format off

// command IDs must be in sync with interop/Command.java
// the largest parameter type must be first

#define RESPONSE_CODE(name) name = ICommand::name

#define COMMON_RESPONSES \
   RESPONSE_CODE(OK), RESPONSE_CODE(INVALID_STATE), RESPONSE_CODE(INTEROP_FAILURE)


enum class StartListeningResponse : int64_t {
   COMMON_RESPONSES,
   RESPONSE_CODE(BLUETOOTH_OFF),
   RESPONSE_CODE(USER_DECLINED),
   RESPONSE_CODE(LISTEN_FAILED),
};
using StartListeningTraits = LongTraits<
   COMMAND_ID(100),
   StartListeningResponse,
   std::string_view /*uuid*/, std::string_view /*name*/, std::chrono::seconds /*discoverability duration*/>;
using StartListening = Base<StartListeningTraits>;


enum class StartDiscoveryResponse : int64_t {
   COMMON_RESPONSES,
   RESPONSE_CODE(NO_BT_ADAPTER),
   RESPONSE_CODE(BLUETOOTH_OFF),
};
using StartDiscoveryTraits = LongTraits<
   COMMAND_ID(101),
   StartDiscoveryResponse,
   std::string_view /*uuid*/, std::string_view /*name*/, bool /*include paired*/>;
using StartDiscovery = Base<StartDiscoveryTraits>;


enum class StopListeningResponse : int64_t {
   COMMON_RESPONSES,
};
using StopListeningTraits = Traits<
   COMMAND_ID(102),
   StopListeningResponse>;
using StopListening = Base<StopListeningTraits>;


enum class StopDiscoveryResponse : int64_t {
   COMMON_RESPONSES,
};
using StopDiscoveryTraits = Traits<
   COMMAND_ID(103),
   StopDiscoveryResponse>;
using StopDiscovery = Base<StopDiscoveryTraits>;


enum class CloseConnectionResponse : int64_t {
   COMMON_RESPONSES,
   RESPONSE_CODE(CONNECTION_NOT_FOUND),
};
using CloseConnectionTraits = Traits<
   COMMAND_ID(104),
   CloseConnectionResponse,
   std::string_view/*error msg*/, std::string_view/*remote mac addr*/>;
using CloseConnection = Base<CloseConnectionTraits>;


enum class EnableBluetoothResponse : int64_t {
   COMMON_RESPONSES,
   RESPONSE_CODE(NO_BT_ADAPTER),
   RESPONSE_CODE(USER_DECLINED),
};
using EnableBluetoothTraits = Traits<
   COMMAND_ID(105),
   EnableBluetoothResponse>;
using EnableBluetooth = Base<EnableBluetoothTraits>;


enum class NegotiationStartResponse : int64_t {
   COMMON_RESPONSES,
};
using NegotiationStartTraits = Traits<
   COMMAND_ID(106),
   NegotiationStartResponse>;
using NegotiationStart = Base<NegotiationStartTraits>;


enum class NegotiationStopResponse : int64_t {
   COMMON_RESPONSES,
};
using NegotiationStopTraits = Traits<
   COMMAND_ID(107),
   NegotiationStopResponse,
   std::string_view/*nominee name*/>;
using NegotiationStop = Base<NegotiationStopTraits>;


enum class SendMessageResponse : int64_t {
   COMMON_RESPONSES,
   RESPONSE_CODE(CONNECTION_NOT_FOUND),
   RESPONSE_CODE(SOCKET_ERROR),
};
using SendMessageTraits = LongTraits<
   COMMAND_ID(108),
   SendMessageResponse,
   std::string_view/*message*/, std::string_view/*remote mac addr*/>;
using SendMessage = Base<SendMessageTraits>;

using SendLongMessageTraits = ExtraLongTraits<
   COMMAND_ID(108),
   SendMessageResponse,
   std::string_view/*message*/, std::string_view/*remote mac addr*/>;
using SendLongMessage = Base<SendLongMessageTraits>;


enum class ShowAndExitResponse : int64_t {
   COMMON_RESPONSES,
};
using ShowAndExitTraits = LongTraits<
   COMMAND_ID(109),
   ShowAndExitResponse,
   std::string_view>;
using ShowAndExit = Base<ShowAndExitTraits>;


enum class ShowToastResponse : int64_t {
   COMMON_RESPONSES,
};
using ShowToastTraits = Traits<
   COMMAND_ID(110),
   ShowToastResponse,
   std::string_view, std::chrono::seconds>;
using ShowToast = Base<ShowToastTraits>;


enum class ShowNotificationResponse : int64_t {
   COMMON_RESPONSES,
};
using ShowNotificationTraits = Traits<
   COMMAND_ID(111),
   ShowNotificationResponse,
   std::string_view>;
using ShowNotification = Base<ShowNotificationTraits>;


enum class ShowRequestResponse : int64_t {
   COMMON_RESPONSES,
};
using ShowRequestTraits = Traits<
   COMMAND_ID(112),
   ShowRequestResponse,
   std::string_view/*type*/, size_t/*size*/, uint32_t/*threshold, 0=not set*/, std::string_view/*name*/>;
using ShowRequest = Base<ShowRequestTraits>;


enum class ShowResponseResponse : int64_t {
   COMMON_RESPONSES,
};
using ShowResponseTraits = LongTraits<
   COMMAND_ID(113),
   ShowResponseResponse,
   dice::Cast/*numbers*/, std::string_view/*type*/, int32_t/*success count, -1=not set*/, std::string_view/*name*/>;
using ShowResponse = Base<ShowResponseTraits>;

using ShowLongResponseTraits = ExtraLongTraits<
   COMMAND_ID(113),
   ShowResponseResponse,
   dice::Cast/*numbers*/, std::string_view/*type*/, int32_t/*success count, -1=not set*/, std::string_view/*name*/>;
using ShowLongResponse = Base<ShowLongResponseTraits>;


enum class ResetGameResponse : int64_t {
   COMMON_RESPONSES,
};
using ResetGameTraits = Traits<
   COMMAND_ID(114),
   ResetGameResponse>;
using ResetGame = Base<ResetGameTraits>;


enum class ResetConnectionsResponse : int64_t {
   COMMON_RESPONSES,
};
using ResetConnectionsTraits = Traits<
   COMMAND_ID(115),
   ResetConnectionsResponse>;
using ResetConnections = Base<ResetConnectionsTraits>;


#undef COMMON_RESPONSES
#undef RESPONSE_CODE


namespace internal {

template <typename... T> struct List {};

using BtDictionary = List<
   EnableBluetooth,
   StartListening,
   StartDiscovery,
   StopListening,
   StopDiscovery,
   CloseConnection,
   SendMessage,
   SendLongMessage,
   ResetConnections>;

using UiDictionary = List<
   NegotiationStart,
   NegotiationStop,
   ShowAndExit,
   ShowToast,
   ShowNotification,
   ShowRequest,
   ShowResponse,
   ShowLongResponse,
   ResetGame>;

// clang-format on

template <typename L, typename T>
struct Contains;

template <typename T>
struct Contains<List<>, T>
{
   static constexpr bool value = false;
};

template <typename T, typename... Ts>
struct Contains<List<T, Ts...>, T>
{
   static constexpr bool value = true;
};

template <typename T, typename U, typename... Ts>
struct Contains<List<U, Ts...>, T>
{
   static constexpr bool value = Contains<List<Ts...>, T>::value;
};

} // namespace internal


template <typename T>
concept BtCommand = internal::Contains<internal::BtDictionary, T>::value;

template <typename T>
concept UiCommand = internal::Contains<internal::UiDictionary, T>::value;

} // namespace cmd

#endif // SIGN_COMMANDS_HPP
