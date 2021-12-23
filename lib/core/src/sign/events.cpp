#include "sign/events.hpp"
#include "bt/device.hpp"
#include "fsm/states.hpp"
#include "dice/serializer.hpp"

namespace event {

template <typename F>
void StateInvoke(F && f, fsm::StateHolder & state)
{
   struct Workaround : F
   {
      using F::operator();
      void operator()(std::monostate) {}
   };
   std::visit(Workaround{std::forward<F>(f)}, state);
}

bool RemoteDeviceConnected::Handle(fsm::StateHolder & stateHolder,
                                   const std::vector<std::string> & args)
{
   // "mac", "name"
   if (args.size() < 2 || args[0].empty())
      return false;
   bt::Device device(args[1], args[0]);
   StateInvoke(
      [&](auto & s) {
         s.OnDeviceConnected(device);
      },
      stateHolder);
   return true;
}

bool RemoteDeviceDisconnected::Handle(fsm::StateHolder & stateHolder,
                                      const std::vector<std::string> & args)
{
   // "mac", "name"
   if (args.size() < 2 || args[0].empty())
      return false;
   bt::Device device(args[1], args[0]);
   StateInvoke(
      [&](auto & s) {
         s.OnDeviceDisconnected(device);
      },
      stateHolder);
   return true;
}

bool ConnectivityEstablished::Handle(fsm::StateHolder & stateHolder,
                                     const std::vector<std::string> &)
{
   StateInvoke(
      [](auto & s) {
         s.OnConnectivityEstablished();
      },
      stateHolder);
   return true;
}

bool NewGameRequested::Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> &)
{
   StateInvoke(
      [](auto & s) {
         s.OnNewGame();
      },
      stateHolder);
   return true;
}

bool MessageReceived::Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args)
{
   // "message", "mac", "name"
   if (args.size() < 3)
      return false;

   bt::Device sender(args[2], args[1]);
   StateInvoke(
      [&](auto & s) {
         s.OnMessageReceived(sender, args[0]);
      },
      stateHolder);
   return true;
}

bool CastRequestIssued::Handle(fsm::StateHolder & stateHolder,
                               const std::vector<std::string> & args)
{
   // "type", "size", "threshold"
   if (args.size() < 2)
      return false;

   try {
      size_t size = std::stoul(args[1]);
      dice::Request request{dice::MakeCast(args[0], size), std::nullopt};
      if (args.size() == 3)
         request.threshold = std::stoi(args[2]);
      StateInvoke(
         [&](auto & s) {
            s.OnCastRequest(std::move(request));
         },
         stateHolder);
   }
   catch (...) {
      return false;
   }
   return true;
}

bool GameStopped::Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> &)
{
   StateInvoke(
      [](auto & s) {
         s.OnGameStopped();
      },
      stateHolder);
   return true;
}

bool BluetoothOn::Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> &)
{
   StateInvoke(
      [](auto & s) {
         s.OnBluetoothOn();
      },
      stateHolder);
   return true;
}

bool BluetoothOff::Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> &)
{
   StateInvoke(
      [](auto & s) {
         s.OnBluetoothOff();
      },
      stateHolder);
   return true;
}

bool SocketReadFailed::Handle(fsm::StateHolder & stateHolder, const std::vector<std::string> & args)
{
   // "mac", "name"
   if (args.size() < 2 || args[0].empty())
      return false;
   bt::Device device(args[1], args[0]);
   StateInvoke(
      [&](auto & s) {
         s.OnSocketReadFailure(device);
      },
      stateHolder);
   return true;
}

} // namespace event