#ifndef FSM_CONTEXT_HPP
#define FSM_CONTEXT_HPP

#include "ctrl/commandadapter.hpp"
#include <variant>

namespace dice {
class IEngine;
class ISerializer;
}
namespace core {
class Timer;
}

namespace fsm {

class StateIdle;
class StateConnecting;
class StatePlaying;
class StateNegotiating;

using StateHolder =
   std::variant<std::monostate, StateIdle, StateConnecting, StateNegotiating, StatePlaying>;

struct Context
{
   dice::IEngine * const generator;
   dice::ISerializer * const serializer;
   core::Timer * const timer;
   core::CommandAdapter proxy;

   StateHolder * const state;
};

} // namespace fsm


#endif // FSM_CONTEXT_HPP
