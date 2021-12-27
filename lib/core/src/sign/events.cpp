#include "sign/events.hpp"
#include "bt/device.hpp"
#include "dice/serializer.hpp"
#include "fsm/statebase.hpp"

namespace event {

bool RemoteDeviceConnected::Handle(fsm::StateBase & s, const std::vector<std::string> & args)
{
   // "mac", "name"
   if (args.size() < 2 || args[0].empty())
      return false;
   bt::Device device{args[1], args[0]};
   s.OnDeviceConnected(device);
   return true;
}

bool RemoteDeviceDisconnected::Handle(fsm::StateBase & s, const std::vector<std::string> & args)
{
   // "mac", "name"
   if (args.size() < 2 || args[0].empty())
      return false;
   bt::Device device{args[1], args[0]};
   s.OnDeviceDisconnected(device);
   return true;
}

bool ConnectivityEstablished::Handle(fsm::StateBase & s, const std::vector<std::string> &)
{
   s.OnConnectivityEstablished();
   return true;
}

bool NewGameRequested::Handle(fsm::StateBase & s, const std::vector<std::string> &)
{
   s.OnNewGame();
   return true;
}

bool MessageReceived::Handle(fsm::StateBase & s, const std::vector<std::string> & args)
{
   // "message", "mac", "name"
   if (args.size() < 3)
      return false;

   bt::Device sender{args[2], args[1]};
   s.OnMessageReceived(sender, args[0]);
   return true;
}

bool CastRequestIssued::Handle(fsm::StateBase & s, const std::vector<std::string> & args)
{
   // "type", "size", "threshold"
   if (args.size() < 2)
      return false;

   try {
      size_t size = std::stoul(args[1]);
      dice::Request request{dice::MakeCast(args[0], size), std::nullopt};
      if (args.size() == 3)
         request.threshold = std::stoi(args[2]);
      s.OnCastRequest(std::move(request));
   }
   catch (...) {
      return false;
   }
   return true;
}

bool GameStopped::Handle(fsm::StateBase & s, const std::vector<std::string> &)
{
   s.OnGameStopped();
   return true;
}

bool BluetoothOn::Handle(fsm::StateBase & s, const std::vector<std::string> &)
{
   s.OnBluetoothOn();
   return true;
}

bool BluetoothOff::Handle(fsm::StateBase & s, const std::vector<std::string> &)
{
   s.OnBluetoothOff();
   return true;
}

bool SocketReadFailed::Handle(fsm::StateBase & s, const std::vector<std::string> & args)
{
   // "mac", "name"
   if (args.size() < 2 || args[0].empty())
      return false;
   bt::Device device{args[1], args[0]};
   s.OnSocketReadFailure(device);
   return true;
}

} // namespace event
