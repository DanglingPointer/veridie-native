#ifndef SIGN_EVENTS_HPP
#define SIGN_EVENTS_HPP

#include "fsm/context.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace event {

using Handler = bool(*)(fsm::StateHolder &, const std::vector<std::string> &);

// event IDs must be in sync with interop/Event.java

struct RemoteDeviceConnected final
{
   static constexpr int32_t ID = 10;
   static constexpr std::string_view NAME = "RemoteDeviceConnected";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct RemoteDeviceDisconnected final
{
   static constexpr int32_t ID = 11;
   static constexpr std::string_view NAME = "RemoteDeviceDisconnected";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct ConnectivityEstablished final
{
   static constexpr int32_t ID = 12;
   static constexpr std::string_view NAME = "ConnectivityEstablished";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct NewGameRequested final
{
   static constexpr int32_t ID = 13;
   static constexpr std::string_view NAME = "NewGameRequested";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct MessageReceived final
{
   static constexpr int32_t ID = 14;
   static constexpr std::string_view NAME = "MessageReceived";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct CastRequestIssued final
{
   static constexpr int32_t ID = 15;
   static constexpr std::string_view NAME = "CastRequestIssued";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct GameStopped final
{
   static constexpr int32_t ID = 16;
   static constexpr std::string_view NAME = "GameStopped";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct BluetoothOn final
{
   static constexpr int32_t ID = 17;
   static constexpr std::string_view NAME = "BluetoothOn";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct BluetoothOff final
{
   static constexpr int32_t ID = 18;
   static constexpr std::string_view NAME = "BluetoothOff";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

struct SocketReadFailed final
{
   static constexpr int32_t ID = 19;
   static constexpr std::string_view NAME = "SocketReadFailed";
   static bool Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args);
};

template<typename... T> struct List {};

using Dictionary = List<
   RemoteDeviceConnected,
   RemoteDeviceDisconnected,
   ConnectivityEstablished,
   NewGameRequested,
   MessageReceived,
   CastRequestIssued,
   GameStopped,
   BluetoothOn,
   BluetoothOff,
   SocketReadFailed>;

} // namespace event

#endif //SIGN_EVENTS_HPP
