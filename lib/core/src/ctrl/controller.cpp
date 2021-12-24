#include "ctrl/controller.hpp"
#include "ctrl/timer.hpp"
#include "dice/engine.hpp"
#include "dice/serializer.hpp"
#include "fsm/context.hpp"
#include "fsm/statebase.hpp"
#include "fsm/stateidle.hpp"
#include "sign/commandmanager.hpp"
#include "sign/externalinvoker.hpp"
#include "sign/events.hpp"

#include "utils/log.hpp"

#include <sstream>
#include <unordered_map>
#include <utility>

namespace {

using EventHandlerMap = std::unordered_map<int32_t, std::pair<std::string_view, event::Handler>>;

template <typename... Events>
EventHandlerMap CreateEventHandlers(event::List<Events...>)
{
   return EventHandlerMap{{Events::ID, {Events::NAME, &Events::Handle}}...};
}

class Controller : public core::IController
{
public:
   Controller(std::unique_ptr<dice::IEngine> engine,
              std::unique_ptr<core::Timer> timer,
              std::unique_ptr<dice::ISerializer> serializer)
      : m_cmdManager(nullptr)
      , m_generator(std::move(engine))
      , m_timer(std::move(timer))
      , m_serializer(std::move(serializer))
      , m_eventHandlers(CreateEventHandlers(event::Dictionary{}))
   {}

private:
   void Start(std::unique_ptr<cmd::IExternalInvoker> uiInvoker,
              std::unique_ptr<cmd::IExternalInvoker> btInvoker) override
   {
      if (m_cmdManager)
         return;
      m_cmdManager = std::make_unique<cmd::Manager>(std::move(uiInvoker), std::move(btInvoker));
      fsm::Context::SwitchToState<fsm::StateIdle>(fsm::Context{m_generator.get(),
                                                               m_serializer.get(),
                                                               m_timer.get(),
                                                               *m_cmdManager,
                                                               &m_state});
   }
   void OnEvent(int32_t eventId, const std::vector<std::string> & args) override
   {
      static constexpr auto TAG = "Event";

      auto it = m_eventHandlers.find(eventId);
      if (it == std::cend(m_eventHandlers)) [[unlikely]] {
         Log::Error(TAG, "Event handler not found, id={}", eventId);
         return;
      }
      std::string_view name = it->second.first;
      event::Handler handler = it->second.second;

      std::ostringstream ss;
      for (const auto & s : args)
         ss << " [" << s << "]";
      Log::Info(TAG, "<<<<< {}{}", name, ss.str());

      if (!m_state) {
         Log::Error(TAG, "{}: no state", __func__);
         return;
      }

      bool success = (*handler)(*m_state, args);
      if (!success) [[unlikely]] {
         Log::Error(TAG, "Could not parse event args");
      }
   }
   void OnCommandResponse(int32_t cmdId, int64_t response) override
   {
      if (!m_cmdManager) [[unlikely]] {
         Log::Error("Command", "{}: no cmd manager", __func__);
         return;
      }
      m_cmdManager->SubmitResponse(cmdId, response);
   }

   std::unique_ptr<cmd::Manager> m_cmdManager;
   std::unique_ptr<dice::IEngine> m_generator;
   std::unique_ptr<core::Timer> m_timer;
   std::unique_ptr<dice::ISerializer> m_serializer;

   EventHandlerMap m_eventHandlers;
   std::unique_ptr<fsm::StateBase> m_state;
};

} // namespace

namespace core {

std::unique_ptr<IController> CreateController(std::unique_ptr<dice::IEngine> engine,
                                              std::unique_ptr<core::Timer> timer,
                                              std::unique_ptr<dice::ISerializer> serializer)
{
   return std::make_unique<Controller>(std::move(engine), std::move(timer), std::move(serializer));
}

} // namespace core
