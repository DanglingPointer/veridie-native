#ifndef FSM_STATEBASE_HPP
#define FSM_STATEBASE_HPP

#include "utils/taskowner.hpp"
#include <string>

namespace bt {
struct Device;
}
namespace dice {
struct Request;
}

namespace fsm {

class StateBase : protected cr::TaskOwner<>
{
protected:
   StateBase() = default;

public:
   void OnBluetoothOn(){}
   void OnBluetoothOff(){}
   void OnDeviceConnected(const bt::Device & /*remote*/){}
   void OnDeviceDisconnected(const bt::Device & /*remote*/){}
   void OnConnectivityEstablished(){}
   void OnNewGame(){}
   void OnMessageReceived(const bt::Device & /*sender*/, const std::string & /*message*/){}
   void OnCastRequest(dice::Request && /*localRequest*/){}
   void OnGameStopped(){}
   void OnSocketReadFailure(const bt::Device & /*transmitter*/){}
   using cr::TaskOwner<>::RethrowExceptions;
};

} // namespace fsm

#endif // FSM_STATEBASE_HPP
