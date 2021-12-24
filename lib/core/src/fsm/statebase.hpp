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
   virtual ~StateBase() = default;
   virtual void OnBluetoothOn(){}
   virtual void OnBluetoothOff(){}
   virtual void OnDeviceConnected(const bt::Device & /*remote*/){}
   virtual void OnDeviceDisconnected(const bt::Device & /*remote*/){}
   virtual void OnConnectivityEstablished(){}
   virtual void OnNewGame(){}
   virtual void OnMessageReceived(const bt::Device & /*sender*/, const std::string & /*message*/){}
   virtual void OnCastRequest(dice::Request && /*localRequest*/){}
   virtual void OnGameStopped(){}
   virtual void OnSocketReadFailure(const bt::Device & /*transmitter*/){}
   using cr::TaskOwner<>::RethrowExceptions;
};

} // namespace fsm

#endif // FSM_STATEBASE_HPP
