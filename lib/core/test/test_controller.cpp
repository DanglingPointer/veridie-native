#include <gtest/gtest.h>
#include <queue>
#include <list>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "fakelogger.hpp"
#include "bt/device.hpp"
#include "ctrl/controller.hpp"
#include "ctrl/timer.hpp"
#include "dice/serializer.hpp"
#include "sign/commandpool.hpp"
#include "sign/externalinvoker.hpp"
#include "sign/events.hpp"
#include "dice/engine.hpp"
#include "dice/serializer.hpp"

namespace fsm {
extern uint32_t g_negotiationRound;
}

namespace {
using namespace std::chrono_literals;

template <typename... Ts>
bool Contains(cmd::internal::List<Ts...>, int32_t id)
{
   return (... || (Ts::ID == id));
}

class MockProxy
{
public:
   struct CmdData
   {
      mem::pool_ptr<cmd::ICommand> c = nullptr;
      int32_t id = 0;
   };

   CmdData PopNextCommand()
   {
      if (m_q.empty())
         return {};
      auto c = std::move(m_q.front());
      m_q.pop();
      return c;
   }
   bool NoCommands() const { return m_q.empty(); }

   std::unique_ptr<cmd::IExternalInvoker> GetUiInvoker()
   {
      struct Invoker : cmd::IExternalInvoker
      {
         Invoker(MockProxy & parent)
            : parent(parent)
         {}
         bool Invoke(mem::pool_ptr<cmd::ICommand> && data, int32_t id) override
         {
            EXPECT_TRUE(Contains(cmd::internal::UiDictionary{}, data->GetId()));
            parent.m_q.push({std::move(data), id});
            return true;
         }

         MockProxy & parent;
      };
      return std::make_unique<Invoker>(*this);
   }

   std::unique_ptr<cmd::IExternalInvoker> GetBtInvoker()
   {
      struct Invoker : cmd::IExternalInvoker
      {
         Invoker(MockProxy & parent)
            : parent(parent)
         {}
         bool Invoke(mem::pool_ptr<cmd::ICommand> && data, int32_t id) override
         {
            EXPECT_TRUE(Contains(cmd::internal::BtDictionary{}, data->GetId()));
            parent.m_q.push({std::move(data), id});
            return true;
         }

         MockProxy & parent;
      };
      return std::make_unique<Invoker>(*this);
   }

private:
   std::queue<CmdData> m_q;
};

class MockTimerEngine
{
public:
   MockTimerEngine()
      : m_processing(false)
   {}
   ~MockTimerEngine() { ExhaustQueue(); }

   void FastForwardTime(std::chrono::seconds sec = 0s)
   {
      if (sec == 0s)
         return ProcessTimers();

      const auto end = m_now + sec;
      do {
         m_now += 1s;
         ProcessTimers();
      } while (m_now < end);
   }

   void DumpTimers()
   {
      fprintf(stderr, "Dumping timers:\n");
      for (const auto & t : m_timers)
         fprintf(stderr,
                 "- Timer scheduled in %lli ms\n",
                 (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t.time - m_now)
                    .count());
   }

   void operator()(std::function<void()> && task, std::chrono::milliseconds period)
   {
      //      fprintf(stderr, "Adding timer %d ms\n", (int)period.count());
      const auto timeToFire = m_now + period;
      m_timers.push_back({std::move(task), timeToFire});
   }

   void ExhaustQueue()
   {
      while (!m_timers.empty())
         FastForwardTime(1s);
   }

private:
   struct Timer
   {
      std::function<void()> task;
      std::chrono::steady_clock::time_point time;
   };
   void ProcessTimers()
   {
      if (m_processing)
         return;
      m_processing = true;
      for (auto it = std::begin(m_timers); it != std::end(m_timers);) {
         if (it->time <= m_now) {
            it->task();
            it = m_timers.erase(it);
         } else {
            ++it;
         }
      }
      m_processing = false;
   }

   std::chrono::steady_clock::time_point m_now;
   std::list<Timer> m_timers;
   bool m_processing;
};

class StubGenerator : public dice::IEngine
{
public:
   uint32_t value = 3;

private:
   void GenerateResult(dice::Cast & cast) override
   {
      cast.Apply([this](auto & vec) {
         for (auto & e : vec)
            e(value);
      });
   }
};

class IdlingFixture : public ::testing::Test
{
protected:
   std::unique_ptr<core::IController> CreateController()
   {
      cmd::pool.ShrinkToFit();
      EXPECT_EQ(0U, cmd::pool.GetBlockCount());
      proxy = &m_proxy;
      timer = &m_timer;
      generator = new StubGenerator;
      auto ctrl = core::CreateController(std::unique_ptr<dice::IEngine>(generator),
                                         std::make_unique<core::Timer>(std::ref(m_timer)),
                                         dice::CreateXmlSerializer());
      ctrl->Start(m_proxy.GetUiInvoker(), m_proxy.GetBtInvoker());
      m_timer.FastForwardTime();
      return ctrl;
   }

   MockProxy m_proxy;
   MockTimerEngine m_timer;

   MockProxy * proxy;
   MockTimerEngine * timer;
   StubGenerator * generator;
   FakeLogger logger;
};

TEST_F(IdlingFixture, state_idle_bluetooth_turned_on_successfully)
{
   auto ctrl = CreateController();
   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());

   logger.Clear();
   auto [c, id] = proxy->PopNextCommand();
   ASSERT_TRUE(c);
   EXPECT_EQ(cmd::EnableBluetooth::ID, c->GetId());
   EXPECT_EQ(0U, c->GetArgsCount());

   // bluetooth on
   ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   ctrl->OnEvent(event::BluetoothOn::ID, {});
   EXPECT_TRUE(proxy->NoCommands());
   logger.Clear();

   // new game requested
   ctrl->OnEvent(event::NewGameRequested::ID, {});
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateConnecting", logger.GetLastStateLine());
}

TEST_F(IdlingFixture, state_idle_bluetooth_fatal_failure)
{
   auto ctrl = CreateController();
   logger.Clear();

   {
      auto [c, id] = proxy->PopNextCommand();
      ASSERT_TRUE(c);
      EXPECT_EQ(cmd::EnableBluetooth::ID, c->GetId());

      ctrl->OnCommandResponse(id, cmd::ICommand::USER_DECLINED);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // no retries
   logger.Clear();
   timer->FastForwardTime(2s);
   EXPECT_TRUE(proxy->NoCommands());
   EXPECT_TRUE(logger.Empty());

   // new game requested
   ctrl->OnEvent(event::NewGameRequested::ID, {});

   {
      auto [c, id] = proxy->PopNextCommand();
      ASSERT_TRUE(c);
      EXPECT_EQ(cmd::EnableBluetooth::ID, c->GetId());
      EXPECT_EQ(0U, c->GetArgsCount());
   }
}
TEST_F(IdlingFixture, state_idle_bluetooth_no_adapter)
{
   auto ctrl = CreateController();
   logger.Clear();

   {
      auto [c, id] = proxy->PopNextCommand();
      ASSERT_TRUE(c);
      EXPECT_EQ(cmd::EnableBluetooth::ID, c->GetId());

      ctrl->OnCommandResponse(id, cmd::ICommand::NO_BT_ADAPTER);
   }
   {
      auto [c, _] = proxy->PopNextCommand();
      ASSERT_TRUE(c);
      EXPECT_EQ(cmd::ShowAndExit::ID, c->GetId());
   }

   timer->FastForwardTime();
   EXPECT_TRUE(logger.GetLastStateLine().empty());
}

TEST_F(IdlingFixture, state_idle_retries_to_enable_bluetooth)
{
   auto ctrl = CreateController();
   logger.Clear();

   {
      auto [c, id] = proxy->PopNextCommand();
      ASSERT_TRUE(c);

      ctrl->OnCommandResponse(id, cmd::ICommand::INVALID_STATE);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // check retry
   timer->FastForwardTime(1s);

   {
      auto [c, id] = proxy->PopNextCommand();
      ASSERT_TRUE(c);
      EXPECT_EQ(cmd::EnableBluetooth::ID, c->GetId());
      EXPECT_EQ(0U, c->GetArgsCount());

      ctrl->OnCommandResponse(id, cmd::ICommand::NO_BT_ADAPTER);
   }

   {
      auto [c, id] = proxy->PopNextCommand();
      ASSERT_TRUE(c);
      EXPECT_EQ(cmd::ShowAndExit::ID, c->GetId());
      EXPECT_EQ(1U, c->GetArgsCount());
      EXPECT_TRUE(proxy->NoCommands());
   }

   // no retries
   logger.Clear();
   timer->FastForwardTime(2s);
   EXPECT_TRUE(proxy->NoCommands());
   EXPECT_TRUE(logger.Empty());

   ctrl->OnEvent(event::NewGameRequested::ID, {});
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(IdlingFixture, state_idle_retry_after_bluetooth_off_and_user_declined)
{
   auto ctrl = CreateController();
   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());
   logger.Clear();
   {
      auto [enableBt, id] = proxy->PopNextCommand();
      ASSERT_TRUE(enableBt);
      EXPECT_EQ(cmd::EnableBluetooth::ID, enableBt->GetId());
      EXPECT_EQ(0U, enableBt->GetArgsCount());
      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   }

   ctrl->OnEvent(event::BluetoothOff::ID, {});
   {
      auto [enableBt, id] = proxy->PopNextCommand();
      ASSERT_TRUE(enableBt);
      EXPECT_EQ(cmd::EnableBluetooth::ID, enableBt->GetId());
      EXPECT_EQ(0U, enableBt->GetArgsCount());
      ctrl->OnCommandResponse(id, cmd::ICommand::USER_DECLINED);
   }
   EXPECT_TRUE(proxy->NoCommands());

   timer->FastForwardTime(2s);
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(event::NewGameRequested::ID, {});
   {
      auto [enableBt, id] = proxy->PopNextCommand();
      ASSERT_TRUE(enableBt);
      EXPECT_EQ(cmd::EnableBluetooth::ID, enableBt->GetId());
      EXPECT_EQ(0U, enableBt->GetArgsCount());
      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   }
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateConnecting", logger.GetLastStateLine());
}

class ConnectingFixture : public IdlingFixture
{
protected:
   ConnectingFixture()
   {
      ctrl = CreateController();
      auto [c, id] = proxy->PopNextCommand();
      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
      ctrl->OnEvent(event::NewGameRequested::ID, {});
      timer->FastForwardTime();
      EXPECT_EQ("New state: StateConnecting", logger.GetLastStateLine());
      EXPECT_TRUE(logger.NoWarningsOrErrors());
      logger.Clear();
   }
   void StartDiscoveryAndListening()
   {
      auto [cmdDiscovering, discId] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdDiscovering);
      EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
      auto [cmdListening, listId] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdListening);
      EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());
      ctrl->OnCommandResponse(discId, cmd::ICommand::OK);
      ctrl->OnCommandResponse(listId, cmd::ICommand::OK);
      EXPECT_TRUE(proxy->NoCommands());
   }
   void RespondOK(int32_t cmdId) { ctrl->OnCommandResponse(cmdId, cmd::ICommand::OK); }

   std::unique_ptr<core::IController> ctrl;
};

TEST_F(ConnectingFixture, discovery_and_listening_started_successfully)
{
   auto [cmdDiscovering, discId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdDiscovering);
   EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
   EXPECT_EQ(3U, cmdDiscovering->GetArgsCount());
   EXPECT_STREQ("true", cmdDiscovering->GetArgAt(2).data());

   auto [cmdListening, listId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdListening);
   EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());
   EXPECT_EQ(3U, cmdListening->GetArgsCount());
   EXPECT_STREQ("300", cmdListening->GetArgAt(2).data());

   ctrl->OnCommandResponse(listId, cmd::ICommand::OK);
   ctrl->OnCommandResponse(discId, cmd::ICommand::OK);
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, retries_to_start_listening_at_least_twice)
{
   {
      auto [cmdDiscovering, discId] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdDiscovering);
      EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
      auto [cmdListening, listId] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdListening);
      EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

      ctrl->OnCommandResponse(listId, cmd::ICommand::INVALID_STATE);
      EXPECT_TRUE(proxy->NoCommands());
   }
   timer->FastForwardTime(1s);
   {
      auto [cmdListening, id] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdListening);
      EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

      ctrl->OnCommandResponse(id, cmd::ICommand::LISTEN_FAILED);
      EXPECT_TRUE(proxy->NoCommands());
   }
   timer->DumpTimers();
   timer->FastForwardTime(1s);
   timer->DumpTimers();
   {
      auto [cmdListening, id] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdListening);
      EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
      EXPECT_TRUE(proxy->NoCommands());
   }
   timer->FastForwardTime(1s);
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, retries_to_start_discovery_at_least_twice)
{
   {
      auto [cmdDiscovering, discId] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdDiscovering);
      EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
      auto [cmdListening, listId] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdListening);
      EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

      ctrl->OnCommandResponse(discId, cmd::ICommand::INVALID_STATE);
      EXPECT_TRUE(proxy->NoCommands());
   }
   timer->FastForwardTime(1s);
   {
      auto [cmdDiscovering, id] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdDiscovering);
      EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());

      ctrl->OnCommandResponse(id, cmd::ICommand::INVALID_STATE);
      EXPECT_TRUE(proxy->NoCommands());
   }
   timer->FastForwardTime(1s);
   {
      auto [cmdDiscovering, id] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdDiscovering);
      EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());

      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
      EXPECT_TRUE(proxy->NoCommands());
   }
   timer->FastForwardTime(1s);
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, fatal_failure_when_both_discovery_and_listening_failed)
{
   auto [cmdDiscovering, discId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdDiscovering);
   EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
   auto [cmdListening, listId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdListening);
   EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

   ctrl->OnCommandResponse(discId, cmd::ICommand::NO_BT_ADAPTER);
   ctrl->OnCommandResponse(listId, cmd::ICommand::USER_DECLINED);
   logger.Clear();

   auto [fatalFailureText, _] = proxy->PopNextCommand();
   ASSERT_TRUE(fatalFailureText);
   EXPECT_EQ(cmd::ShowAndExit::ID, fatalFailureText->GetId());
   EXPECT_TRUE(logger.GetLastStateLine().empty());
   timer->ExhaustQueue();
}

TEST_F(ConnectingFixture, no_fatal_failure_when_only_listening_failed)
{
   auto [cmdDiscovering, discId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdDiscovering);
   EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
   auto [cmdListening, listId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdListening);
   EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

   ctrl->OnCommandResponse(discId, cmd::ICommand::OK);
   ctrl->OnCommandResponse(listId, cmd::ICommand::LISTEN_FAILED);
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, no_fatal_failure_when_only_discovery_failed)
{
   auto [cmdDiscovering, discId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdDiscovering);
   EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
   auto [cmdListening, listId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdListening);
   EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

   ctrl->OnCommandResponse(discId, cmd::ICommand::INVALID_STATE);
   ctrl->OnCommandResponse(listId, cmd::ICommand::OK);
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, goes_to_idle_and_back_if_bluetooth_is_off)
{
   auto [cmdDiscovering, discId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdDiscovering);
   EXPECT_EQ(cmd::StartDiscovery::ID, cmdDiscovering->GetId());
   auto [cmdListening, listId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdListening);
   EXPECT_EQ(cmd::StartListening::ID, cmdListening->GetId());

   size_t prevBlockCount = cmd::pool.GetBlockCount();
   ctrl->OnCommandResponse(discId, cmd::ICommand::BLUETOOTH_OFF);
   ctrl->OnCommandResponse(listId, cmd::ICommand::BLUETOOTH_OFF);
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());
   EXPECT_TRUE(cmd::pool.GetBlockCount() <= prevBlockCount);
   auto [enableBt, id] = proxy->PopNextCommand();
   ASSERT_TRUE(enableBt);
   EXPECT_EQ(cmd::EnableBluetooth::ID, enableBt->GetId());
   EXPECT_TRUE(proxy->NoCommands());
   ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateConnecting", logger.GetLastStateLine());
}

TEST_F(ConnectingFixture, sends_hello_to_connected_device)
{
   StartDiscoveryAndListening();

   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:49", "Chalie Chaplin"});

   auto [cmdHello, helloId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdHello);
   EXPECT_EQ(cmd::SendMessage::ID, cmdHello->GetId());
   EXPECT_EQ(2U, cmdHello->GetArgsCount());
   EXPECT_STREQ(R"(<Hello><Mac>5c:b9:01:f8:b6:49</Mac></Hello>)", cmdHello->GetArgAt(0).data());
   EXPECT_STREQ("5c:b9:01:f8:b6:49", cmdHello->GetArgAt(1).data());
   ctrl->OnCommandResponse(helloId, cmd::ICommand::OK);

   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, retries_hello_on_invalid_state_and_disconnects_on_socket_error)
{
   StartDiscoveryAndListening();

   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:49", "Chalie Chaplin"});

   {
      auto [hello, id] = proxy->PopNextCommand();
      ASSERT_TRUE(hello);
      EXPECT_EQ(cmd::SendMessage::ID, hello->GetId());
      ctrl->OnCommandResponse(id, cmd::ICommand::INVALID_STATE);
   }
   {
      auto [hello, id] = proxy->PopNextCommand();
      ASSERT_TRUE(hello);
      EXPECT_EQ(cmd::SendMessage::ID, hello->GetId());
      ctrl->OnCommandResponse(id, cmd::ICommand::SOCKET_ERROR);
   }

   auto [disconnect, id] = proxy->PopNextCommand();
   ASSERT_TRUE(disconnect);
   EXPECT_EQ(cmd::CloseConnection::ID, disconnect->GetId());
   EXPECT_EQ(2U, disconnect->GetArgsCount());
   EXPECT_STREQ("5c:b9:01:f8:b6:49", disconnect->GetArgAt(1).data());
   ctrl->OnCommandResponse(id, cmd::ICommand::OK);

   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, disconnects_on_read_error_and_does_not_retry_hello)
{
   StartDiscoveryAndListening();

   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:49", "Chalie Chaplin"});

   auto [hello, helloId] = proxy->PopNextCommand();
   ASSERT_TRUE(hello);
   EXPECT_EQ(cmd::SendMessage::ID, hello->GetId());
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(event::SocketReadFailed::ID, {"5c:b9:01:f8:b6:49", ""});
   auto [disconnect, disconnectId] = proxy->PopNextCommand();
   ASSERT_TRUE(disconnect);
   EXPECT_EQ(cmd::CloseConnection::ID, disconnect->GetId());
   EXPECT_EQ(2U, disconnect->GetArgsCount());
   EXPECT_STREQ("5c:b9:01:f8:b6:49", disconnect->GetArgAt(1).data());
   ctrl->OnCommandResponse(disconnectId, cmd::ICommand::OK);

   ctrl->OnCommandResponse(helloId, cmd::ICommand::INVALID_STATE);
   EXPECT_TRUE(proxy->NoCommands());
}

TEST_F(ConnectingFixture, does_not_start_negotiation_until_received_own_mac)
{
   StartDiscoveryAndListening();

   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:49", "Chalie Chaplin"});

   auto [cmdHello, helloId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdHello);
   EXPECT_EQ(cmd::SendMessage::ID, cmdHello->GetId());
   ctrl->OnCommandResponse(helloId, cmd::ICommand::OK);
   logger.Clear();

   ctrl->OnEvent(event::ConnectivityEstablished::ID, {}); // start
   auto [toast, toastId] = proxy->PopNextCommand();
   ASSERT_TRUE(toast);
   EXPECT_EQ(cmd::ShowToast::ID, toast->GetId());
   EXPECT_EQ(2U, toast->GetArgsCount());
   EXPECT_STREQ("3", toast->GetArgAt(1).data());
   EXPECT_NE("New state: StateNegotiating", logger.GetLastStateLine());
   logger.Clear();

   timer->FastForwardTime(1s);
   EXPECT_TRUE(proxy->NoCommands());
   EXPECT_TRUE(logger.Empty());

   ctrl->OnEvent(event::MessageReceived::ID,
                 {R"(<Hello><Mac>f4:06:69:7b:4b:e7</Mac></Hello>)", "5c:b9:01:f8:b6:49", ""});
   EXPECT_TRUE(proxy->NoCommands());
   logger.Clear();

   timer->FastForwardTime(1s);
   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());

   auto stopDiscovery = proxy->PopNextCommand().c;
   ASSERT_TRUE(stopDiscovery);
   EXPECT_EQ(cmd::StopDiscovery::ID, stopDiscovery->GetId());

   auto stopListening = proxy->PopNextCommand().c;
   ASSERT_TRUE(stopListening);
   EXPECT_EQ(cmd::StopListening::ID, stopListening->GetId());
}

TEST_F(ConnectingFixture, goes_to_idle_on_game_stop)
{
   StartDiscoveryAndListening();

   ctrl->OnEvent(event::GameStopped::ID, {}); // game stopped

   auto resetBt = proxy->PopNextCommand().c;
   ASSERT_TRUE(resetBt);
   EXPECT_EQ(cmd::ResetConnections::ID, resetBt->GetId());
   EXPECT_EQ(0U, resetBt->GetArgsCount());

   timer->FastForwardTime();

   auto stopDiscovery = proxy->PopNextCommand().c;
   ASSERT_TRUE(stopDiscovery);
   EXPECT_EQ(cmd::StopDiscovery::ID, stopDiscovery->GetId());

   auto stopListening = proxy->PopNextCommand().c;
   ASSERT_TRUE(stopListening);
   EXPECT_EQ(cmd::StopListening::ID, stopListening->GetId());

   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());
}

TEST_F(ConnectingFixture, does_not_negotiate_with_disconnected)
{
   fsm::g_negotiationRound = 1;
   StartDiscoveryAndListening();

   // 3 well-behaved peers
   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:41", "Chalie Chaplin 1"});
   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:42", "Chalie Chaplin 2"});
   ctrl->OnEvent(event::RemoteDeviceConnected::ID, {"5c:b9:01:f8:b6:43", "Chalie Chaplin 3"});

   for (int i = 0; i < 3; ++i) {
      auto [cmdHello, id] = proxy->PopNextCommand();
      ASSERT_TRUE(cmdHello);
      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   }

   // 1 weird peer
   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Hello><Mac>5c:b9:01:f8:b6:40</Mac></Hello>)", "5c:b9:01:f8:b6:44", "Charlie Chaplin 4"});
   auto [cmdHello, helloId] = proxy->PopNextCommand();
   ASSERT_TRUE(cmdHello);
   EXPECT_EQ(cmd::SendMessage::ID, cmdHello->GetId());
   EXPECT_EQ(2U, cmdHello->GetArgsCount());
   EXPECT_STREQ(R"(<Hello><Mac>5c:b9:01:f8:b6:44</Mac></Hello>)", cmdHello->GetArgAt(0).data());
   EXPECT_STREQ("5c:b9:01:f8:b6:44", cmdHello->GetArgAt(1).data());
   ctrl->OnCommandResponse(helloId, cmd::ICommand::OK);

   // 1 disconnects
   ctrl->OnEvent(event::SocketReadFailed::ID, {"5c:b9:01:f8:b6:42", ""});
   auto [disconnect, disconnectId] = proxy->PopNextCommand();
   ASSERT_TRUE(disconnect);
   EXPECT_EQ(cmd::CloseConnection::ID, disconnect->GetId());
   EXPECT_EQ(2U, disconnect->GetArgsCount());
   EXPECT_STREQ("5c:b9:01:f8:b6:42", disconnect->GetArgAt(1).data());
   ctrl->OnCommandResponse(disconnectId, cmd::ICommand::OK);

   EXPECT_TRUE(proxy->NoCommands());
   logger.Clear();

   // game start
   ctrl->OnEvent(event::ConnectivityEstablished::ID, {});
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());

   auto stopDiscovery = proxy->PopNextCommand().c;
   ASSERT_TRUE(stopDiscovery);
   auto stopListening = proxy->PopNextCommand().c;
   ASSERT_TRUE(stopListening);

   auto [negotiationStart, negId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   ctrl->OnCommandResponse(negId, cmd::ICommand::OK);

   // sorted list of candidates should be:
   // 5c:b9:01:f8:b6:40
   // 5c:b9:01:f8:b6:41
   // 5c:b9:01:f8:b6:43 <-- round 2
   // 5c:b9:01:f8:b6:44
   const char * expectedOffer = R"(<Offer round="2"><Mac>5c:b9:01:f8:b6:43</Mac></Offer>)";
   ASSERT_EQ(2u, fsm::g_negotiationRound);

   // local offer is being broadcast, the order is compiler-dependent
   std::vector<mem::pool_ptr<cmd::ICommand>> cmds;
   for (int i = 0; i < 3; ++i) {
      auto [offer, id] = proxy->PopNextCommand();
      ASSERT_TRUE(offer);
      EXPECT_EQ(cmd::SendMessage::ID, offer->GetId());
      EXPECT_EQ(2U, offer->GetArgsCount());
      EXPECT_STREQ(expectedOffer, offer->GetArgAt(0).data());

      cmds.push_back(std::move(offer));
      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   }
   {
      const auto it41 = std::find_if(cmds.cbegin(), cmds.cend(), [](const auto & offer) {
         return offer->GetArgAt(1) == "5c:b9:01:f8:b6:41";
      });
      EXPECT_NE(cmds.cend(), it41);
   }
   {
      const auto it43 = std::find_if(cmds.cbegin(), cmds.cend(), [](const auto & offer) {
         return offer->GetArgAt(1) == "5c:b9:01:f8:b6:43";
      });
      EXPECT_NE(cmds.cend(), it43);
   }
   {
      const auto it44 = std::find_if(cmds.cbegin(), cmds.cend(), [](const auto & offer) {
         return offer->GetArgAt(1) == "5c:b9:01:f8:b6:44";
      });
      EXPECT_NE(cmds.cend(), it44);
   }
   EXPECT_TRUE(proxy->NoCommands());
}

template <size_t PEERS_COUNT>
class NegotiatingFixture : public ConnectingFixture
{
protected:
   static constexpr size_t peersCount = PEERS_COUNT;

   NegotiatingFixture(std::optional<uint32_t> round = std::nullopt)
      : ConnectingFixture()
      , localMac("5c:b9:01:f8:b6:4" + std::to_string(peersCount))
   {
      if (round)
         fsm::g_negotiationRound = *round;

      StartDiscoveryAndListening();

      for (size_t i = 0; i < peersCount; ++i)
         peers.emplace_back("Charlie Chaplin " + std::to_string(i),
                            "5c:b9:01:f8:b6:4" + std::to_string(i));

      for (const auto & peer : peers) {
         ctrl->OnEvent(event::RemoteDeviceConnected::ID, {peer.mac, peer.name});
         auto [cmdHello, id] = proxy->PopNextCommand();
         EXPECT_TRUE(cmdHello);
         EXPECT_EQ(cmd::SendMessage::ID, cmdHello->GetId());
         RespondOK(id);
      }
      for (const auto & peer : peers) {
         ctrl->OnEvent(event::MessageReceived::ID,
                       {R"(<Hello><Mac>)" + localMac + R"(</Mac></Hello>)", peer.mac, peer.name});
         EXPECT_TRUE(proxy->NoCommands());
      }
      ctrl->OnEvent(event::ConnectivityEstablished::ID, {}); // start
      timer->FastForwardTime();
      EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());

      auto stopDiscovery = proxy->PopNextCommand().c;
      EXPECT_TRUE(stopDiscovery);
      EXPECT_EQ(cmd::StopDiscovery::ID, stopDiscovery->GetId());

      auto stopListening = proxy->PopNextCommand().c;
      EXPECT_TRUE(stopListening);
      EXPECT_EQ(cmd::StopListening::ID, stopListening->GetId());
      logger.Clear();
   }
   void CheckLocalOffer(const char * expectedOffer)
   {
      std::unordered_set<std::string> receiverMacs;
      for (auto it = std::crbegin(Peers()); it != std::crend(Peers()); ++it) {
         auto [offer, id] = proxy->PopNextCommand();
         ASSERT_TRUE(offer);
         EXPECT_EQ(cmd::SendMessage::ID, offer->GetId());
         EXPECT_EQ(2U, offer->GetArgsCount());
         EXPECT_STREQ(expectedOffer, offer->GetArgAt(0).data());
         receiverMacs.emplace(offer->GetArgAt(1));
         RespondOK(id);
      }
      for (const auto & device : Peers()) {
         EXPECT_TRUE(receiverMacs.contains(device.mac));
      }
   }

   const std::vector<bt::Device> & Peers() const { return peers; }
   const std::string localMac;

private:
   std::vector<bt::Device> peers;
};

#define NEG_FIX_ALIAS(num) using NegotiatingFixture##num = NegotiatingFixture<num>

NEG_FIX_ALIAS(2);
NEG_FIX_ALIAS(3);
NEG_FIX_ALIAS(4);
NEG_FIX_ALIAS(5);
NEG_FIX_ALIAS(6);
NEG_FIX_ALIAS(7);
NEG_FIX_ALIAS(8);
NEG_FIX_ALIAS(9);
NEG_FIX_ALIAS(10);

#undef NEG_FIX_ALIAS

// round 3
TEST_F(NegotiatingFixture10, goes_to_negotiation_successfully)
{
   SUCCEED();
}

// round 4
TEST_F(NegotiatingFixture4, increases_round_appropriately)
{
   fsm::g_negotiationRound = 4;

   auto [negotiationStart, negId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   ctrl->OnCommandResponse(negId, cmd::ICommand::OK);

   // sorted list of candidates should be:
   // 5c:b9:01:f8:b6:40 <-- round 5
   // 5c:b9:01:f8:b6:41 <-- round 6
   // 5c:b9:01:f8:b6:42
   // 5c:b9:01:f8:b6:43
   // 5c:b9:01:f8:b6:44 <-- round 4
   CheckLocalOffer(R"(<Offer round="4"><Mac>5c:b9:01:f8:b6:44</Mac></Offer>)");
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="5"><Mac>5c:b9:01:f8:b6:40</Mac></Offer>)", "5c:b9:01:f8:b6:42", ""});
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="3"><Mac>5c:b9:01:f8:b6:43</Mac></Offer>)", "5c:b9:01:f8:b6:41", ""});
   EXPECT_TRUE(proxy->NoCommands());

   timer->FastForwardTime(1s);
   CheckLocalOffer(R"(<Offer round="5"><Mac>5c:b9:01:f8:b6:40</Mac></Offer>)");
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="6"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)", "5c:b9:01:f8:b6:40", ""});
   EXPECT_TRUE(proxy->NoCommands());

   timer->FastForwardTime(1s);
   CheckLocalOffer(R"(<Offer round="6"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)");
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="6"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)", "5c:b9:01:f8:b6:41", ""});
   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="6"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)", "5c:b9:01:f8:b6:42", ""});
   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="6"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)", "5c:b9:01:f8:b6:43", ""});

   timer->FastForwardTime(1s);
   auto [negotiationStop, stopId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStop);
   EXPECT_EQ(cmd::NegotiationStop::ID, negotiationStop->GetId());
   EXPECT_EQ(1U, negotiationStop->GetArgsCount());
   EXPECT_STREQ("Charlie Chaplin 1", negotiationStop->GetArgAt(0).data());
   EXPECT_EQ("New state: StatePlaying", logger.GetLastStateLine());
   ctrl->OnCommandResponse(stopId, cmd::ICommand::OK);
   EXPECT_TRUE(proxy->NoCommands());
}

// round 7
TEST_F(NegotiatingFixture2, handles_disconnects_and_disagreements_on_nominees_mac)
{
   fsm::g_negotiationRound = 7;

   auto [negotiationStart, negId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   ctrl->OnCommandResponse(negId, cmd::ICommand::OK);

   // sorted list of candidates should be:
   // 5c:b9:01:f8:b6:40
   // 5c:b9:01:f8:b6:41 <-- round 7
   // 5c:b9:01:f8:b6:42
   CheckLocalOffer(R"(<Offer round="7"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)");
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="7"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)", "5c:b9:01:f8:b6:41", ""});
   // peer 1 disconnected from peer 0
   ctrl->OnEvent(
      event::MessageReceived::ID,
      {R"(<Offer round="7"><Mac>5c:b9:01:f8:b6:42</Mac></Offer>)", "5c:b9:01:f8:b6:40", ""});

   timer->FastForwardTime(1s);
   CheckLocalOffer(R"(<Offer round="7"><Mac>5c:b9:01:f8:b6:41</Mac></Offer>)");
   EXPECT_TRUE(proxy->NoCommands());

   // peer 1 disconnected from us as well
   ctrl->OnEvent(event::SocketReadFailed::ID, {"5c:b9:01:f8:b6:41", ""});
   auto [disconnect, disconnectId] = proxy->PopNextCommand();
   ASSERT_TRUE(disconnect);
   EXPECT_EQ(cmd::CloseConnection::ID, disconnect->GetId());
   EXPECT_EQ(2U, disconnect->GetArgsCount());
   EXPECT_STREQ("5c:b9:01:f8:b6:41", disconnect->GetArgAt(1).data());
   ctrl->OnCommandResponse(disconnectId, cmd::ICommand::OK);
   EXPECT_TRUE(proxy->NoCommands());

   timer->FastForwardTime(1s);
   const char * expectedOffer = R"(<Offer round="7"><Mac>5c:b9:01:f8:b6:42</Mac></Offer>)";
   {
      auto [offer, id] = proxy->PopNextCommand();
      ASSERT_TRUE(offer);
      EXPECT_EQ(cmd::SendMessage::ID, offer->GetId());
      EXPECT_EQ(2U, offer->GetArgsCount());
      EXPECT_STREQ(expectedOffer, offer->GetArgAt(0).data());
      EXPECT_STREQ("5c:b9:01:f8:b6:40", offer->GetArgAt(1).data());
      ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   }
   EXPECT_TRUE(proxy->NoCommands());

   timer->FastForwardTime(1s);
   auto [negotiationStop, id] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStop);
   EXPECT_EQ(cmd::NegotiationStop::ID, negotiationStop->GetId());
   EXPECT_EQ(1U, negotiationStop->GetArgsCount());
   EXPECT_STREQ("You", negotiationStop->GetArgAt(0).data());
   EXPECT_EQ("New state: StatePlaying", logger.GetLastStateLine());
   ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   EXPECT_TRUE(proxy->NoCommands());
}

// round 8
TEST_F(NegotiatingFixture3, goes_to_idle_on_game_stopped)
{
   fsm::g_negotiationRound = 8;

   auto [negotiationStart, negStartId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   ctrl->OnCommandResponse(negStartId, cmd::ICommand::OK);

   CheckLocalOffer(R"(<Offer round="8"><Mac>5c:b9:01:f8:b6:40</Mac></Offer>)");
   EXPECT_TRUE(proxy->NoCommands());

   ctrl->OnEvent(event::GameStopped::ID, {});

   auto [resetBt, id] = proxy->PopNextCommand();
   ASSERT_TRUE(resetBt);
   EXPECT_EQ(cmd::ResetConnections::ID, resetBt->GetId());
   EXPECT_EQ(0U, resetBt->GetArgsCount());
   ctrl->OnCommandResponse(id, cmd::ICommand::OK);
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());
}

template <size_t PEERS_COUNT, uint32_t ROUND>
class PlayingFixture : public NegotiatingFixture<PEERS_COUNT>
{
protected:
   static constexpr uint32_t round = ROUND;

   using Base = NegotiatingFixture<PEERS_COUNT>;

   PlayingFixture()
      : Base(round - 1)
   {
      auto [negotiationStart, negId] = Base::proxy->PopNextCommand();
      EXPECT_TRUE(negotiationStart);
      EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
      Base::RespondOK(negId);

      size_t nomineeIndex = round % (Base::Peers().size() + 1);
      nomineeMac =
         nomineeIndex == Base::Peers().size() ? Base::localMac : Base::Peers()[nomineeIndex].mac;
      nomineeName = nomineeIndex == Base::Peers().size() ? "You" : Base::Peers()[nomineeIndex].name;

      std::string offer =
         "<Offer round=\"" + std::to_string(round) + "\"><Mac>" + nomineeMac + "</Mac></Offer>";
      Base::CheckLocalOffer(offer.c_str());
      for (const auto & device : Base::Peers())
         Base::ctrl->OnEvent(event::MessageReceived::ID, {offer, device.mac, device.name});

      Base::timer->FastForwardTime(1s);
      auto [negotiationStop, stopId] = Base::proxy->PopNextCommand();
      EXPECT_TRUE(negotiationStop);
      EXPECT_EQ(cmd::NegotiationStop::ID, negotiationStop->GetId());
      EXPECT_EQ(1U, negotiationStop->GetArgsCount());
      EXPECT_STREQ(nomineeName.c_str(), negotiationStop->GetArgAt(0).data());
      EXPECT_EQ("New state: StatePlaying", Base::logger.GetLastStateLine());
      Base::RespondOK(stopId);
      Base::logger.Clear();
      EXPECT_TRUE(Base::proxy->NoCommands());
   }

   std::string nomineeMac;
   std::string nomineeName;
   const std::unique_ptr<dice::ISerializer> serializer = dice::CreateXmlSerializer();
};

dice::Cast CastFilledWith(uint32_t value, const std::string & type, size_t size)
{
   auto cast = dice::MakeCast(type, size);
   cast.Apply([value](auto & vec) {
      for (auto & v : vec)
         v(value);
   });
   return cast;
}

using P2R8 = PlayingFixture<2u, 8u>;

TEST_F(P2R8, local_generator_responds_to_remote_and_local_requests)
{
   timer->FastForwardTime(2s);
   EXPECT_TRUE(proxy->NoCommands());

   // remote request
   {
      generator->value = 3;
      ctrl->OnEvent(event::MessageReceived::ID,
                    {R"(<Request type="D6" size="4" successFrom="3" />)", Peers()[0].mac, ""});

      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D6", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("4", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("3", showRequest->GetArgAt(2).data());
      EXPECT_STREQ(Peers()[0].name.c_str(), showRequest->GetArgAt(3).data());
      RespondOK(showReqId);

      const auto expectedResponse = dice::Response{CastFilledWith(3, "D6", 4), 4u};
      for (const auto & peer : Peers()) {
         auto [sendResponse, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendResponse);
         EXPECT_EQ(cmd::SendMessage::ID, sendResponse->GetId());
         EXPECT_EQ(2U, sendResponse->GetArgsCount());

         const auto actualResponse = serializer->Deserialize(sendResponse->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Response>(actualResponse));
         EXPECT_EQ(expectedResponse, std::get<dice::Response>(actualResponse));

         EXPECT_STREQ(peer.mac.c_str(), sendResponse->GetArgAt(1).data());
         RespondOK(id);
      }

      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ("3;3;3;3;", showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D6", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("4", showResponse->GetArgAt(2).data());
      EXPECT_STREQ("You", showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // local request w threshold
   {
      generator->value = 42;
      ctrl->OnEvent(event::CastRequestIssued::ID, {"D100", "2", "43"});

      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D100", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("2", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("43", showRequest->GetArgAt(2).data());
      EXPECT_STREQ("You", showRequest->GetArgAt(3).data());
      RespondOK(showReqId);

      const auto expectedRequest = dice::Request{dice::MakeCast("D100", 2), 43};
      for (const auto & peer : Peers()) {
         auto [sendRequest, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendRequest);
         EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
         EXPECT_EQ(2U, sendRequest->GetArgsCount());

         const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
         EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

         EXPECT_STREQ(peer.mac.c_str(), sendRequest->GetArgAt(1).data());
         RespondOK(id);
      }

      const auto expectedResponse = dice::Response{CastFilledWith(42, "D100", 2), 0u};
      for (const auto & peer : Peers()) {
         auto [sendResponse, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendResponse);
         EXPECT_EQ(cmd::SendMessage::ID, sendResponse->GetId());
         EXPECT_EQ(2U, sendResponse->GetArgsCount());

         const auto actualResponse = serializer->Deserialize(sendResponse->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Response>(actualResponse));
         EXPECT_EQ(expectedResponse, std::get<dice::Response>(actualResponse));

         EXPECT_STREQ(peer.mac.c_str(), sendResponse->GetArgAt(1).data());
         RespondOK(id);
      }

      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ("42;42;", showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D100", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("0", showResponse->GetArgAt(2).data());
      EXPECT_STREQ("You", showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // local request w/o threshold
   {
      generator->value = 42;
      ctrl->OnEvent(event::CastRequestIssued::ID, {"D100", "2"});

      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D100", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("2", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("0", showRequest->GetArgAt(2).data());
      EXPECT_STREQ("You", showRequest->GetArgAt(3).data());
      RespondOK(showReqId);

      const auto expectedRequest = dice::Request{dice::MakeCast("D100", 2), std::nullopt};
      for (const auto & peer : Peers()) {
         auto [sendRequest, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendRequest);
         EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
         EXPECT_EQ(2U, sendRequest->GetArgsCount());

         const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
         EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

         EXPECT_STREQ(peer.mac.c_str(), sendRequest->GetArgAt(1).data());
         RespondOK(id);
      }

      const auto expectedResponse = dice::Response{CastFilledWith(42, "D100", 2), std::nullopt};
      for (const auto & peer : Peers()) {
         auto [sendResponse, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendResponse);
         EXPECT_EQ(cmd::SendMessage::ID, sendResponse->GetId());
         EXPECT_EQ(2U, sendResponse->GetArgsCount());

         const auto actualResponse = serializer->Deserialize(sendResponse->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Response>(actualResponse));
         EXPECT_EQ(expectedResponse, std::get<dice::Response>(actualResponse));

         EXPECT_STREQ(peer.mac.c_str(), sendResponse->GetArgAt(1).data());
         RespondOK(id);
      }

      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ("42;42;", showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D100", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("-1", showResponse->GetArgAt(2).data());
      EXPECT_STREQ("You", showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // BIG local request
   {
      generator->value = 6;
      ctrl->OnEvent(event::CastRequestIssued::ID, {"D6", "70", "3"});

      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D6", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("70", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("3", showRequest->GetArgAt(2).data());
      EXPECT_STREQ("You", showRequest->GetArgAt(3).data());
      RespondOK(showReqId);

      const auto expectedRequest = dice::Request{dice::MakeCast("D6", 70), 3u};
      for (const auto & peer : Peers()) {
         auto [sendRequest, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendRequest);
         EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
         EXPECT_EQ(2U, sendRequest->GetArgsCount());

         const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
         EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

         EXPECT_STREQ(peer.mac.c_str(), sendRequest->GetArgAt(1).data());
         RespondOK(id);
      }

      const auto expectedResponse = dice::Response{CastFilledWith(6, "D6", 70), 70};
      for (const auto & peer : Peers()) {
         auto [sendResponse, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendResponse);
         EXPECT_EQ(cmd::SendMessage::ID, sendResponse->GetId());
         EXPECT_EQ(2U, sendResponse->GetArgsCount());

         const auto actualResponse = serializer->Deserialize(sendResponse->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Response>(actualResponse));
         EXPECT_EQ(expectedResponse, std::get<dice::Response>(actualResponse));

         EXPECT_STREQ(peer.mac.c_str(), sendResponse->GetArgAt(1).data());
         RespondOK(id);
      }

      std::ostringstream expectedTextResponse;
      for (int i = 0; i < 70; ++i)
         expectedTextResponse << "6;";

      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ(expectedTextResponse.str().c_str(), showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D6", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("70", showResponse->GetArgAt(2).data());
      EXPECT_STREQ("You", showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // Peer 1 wants to re-negotiate before 5 sec
   std::string offer = R"(<Offer round="12"><Mac>)" + Peers()[0].mac + "</Mac></Offer>";
   ctrl->OnEvent(event::MessageReceived::ID, {offer, Peers()[1].mac, ""});
   EXPECT_TRUE(proxy->NoCommands());

   // Peer 1 tries again 3 sec later
   timer->FastForwardTime(8s);
   EXPECT_TRUE(proxy->NoCommands());
   ctrl->OnEvent(event::MessageReceived::ID, {offer, Peers()[1].mac, ""});

   timer->FastForwardTime();
   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());
   auto [negotiationStart, negStartId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   RespondOK(negStartId);

   for (auto it = std::crbegin(Peers()); it != std::crend(Peers()); ++it) {
      auto [sendOffer, id] = proxy->PopNextCommand();
      ASSERT_TRUE(sendOffer);
      EXPECT_EQ(cmd::SendMessage::ID, sendOffer->GetId());
      EXPECT_EQ(2U, sendOffer->GetArgsCount());
      EXPECT_STREQ(offer.c_str(), sendOffer->GetArgAt(0).data());
      EXPECT_STREQ(it->mac.c_str(), sendOffer->GetArgAt(1).data());
      RespondOK(id);
   }
   EXPECT_TRUE(proxy->NoCommands());
}

using P2R13 = PlayingFixture<2u, 13u>;

TEST_F(P2R13, remote_generator_is_respected)
{
   // remote request
   {
      // peer 0 makes a request
      ctrl->OnEvent(event::MessageReceived::ID,
                    {R"(<Request type="D8" size="1" />)", Peers()[0].mac, ""});
      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D8", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("1", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("0", showRequest->GetArgAt(2).data());
      EXPECT_STREQ(Peers()[0].name.c_str(), showRequest->GetArgAt(3).data());
      RespondOK(showReqId);
      EXPECT_TRUE(proxy->NoCommands());

      timer->FastForwardTime(1s);
      EXPECT_TRUE(proxy->NoCommands());

      // peer 0 answers its own request even though it's not generator
      ctrl->OnEvent(
         event::MessageReceived::ID,
         {R"(<Response size="1" type="D8"><Val>8</Val></Response>)", Peers()[0].mac, ""});
      EXPECT_TRUE(proxy->NoCommands());

      // generator answers the request
      ctrl->OnEvent(
         event::MessageReceived::ID,
         {R"(<Response size="1" type="D8"><Val>1</Val></Response> )", Peers()[1].mac, ""});
      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ("1;", showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D8", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("-1", showResponse->GetArgAt(2).data());
      EXPECT_STREQ(Peers()[1].name.c_str(), showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // repeating local request
   {
      ctrl->OnEvent(event::CastRequestIssued::ID, {"D4", "1", "3"});
      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D4", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("1", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("3", showRequest->GetArgAt(2).data());
      EXPECT_STREQ("You", showRequest->GetArgAt(3).data());
      RespondOK(showReqId);

      const auto expectedRequest = dice::Request{dice::MakeCast("D4", 1), 3u};
      for (const auto & peer : Peers()) {
         auto [sendRequest, id] = proxy->PopNextCommand();
         ASSERT_TRUE(sendRequest);
         EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
         EXPECT_EQ(2U, sendRequest->GetArgsCount());

         const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
         EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
         EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

         EXPECT_STREQ(peer.mac.c_str(), sendRequest->GetArgAt(1).data());
         RespondOK(id);
      }
      EXPECT_TRUE(proxy->NoCommands());

      timer->FastForwardTime(1s);

      auto [sendRequest, sendReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(sendRequest);
      EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
      EXPECT_EQ(2U, sendRequest->GetArgsCount());

      const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
      EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
      EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

      EXPECT_STREQ(Peers()[1].mac.c_str(), sendRequest->GetArgAt(1).data());
      RespondOK(sendReqId);
      EXPECT_TRUE(proxy->NoCommands());

      ctrl->OnEvent(event::MessageReceived::ID,
                    {R"(<Response successCount="1" size="1" type="D4"><Val>4</Val></Response>)",
                     Peers()[1].mac,
                     ""});
      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ("4;", showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D4", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("1", showResponse->GetArgAt(2).data());
      EXPECT_STREQ(Peers()[1].name.c_str(), showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());

      timer->FastForwardTime(1s);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // 7 more requests and responses
   for (int i = 0; i < 7; ++i) {
      ctrl->OnEvent(event::MessageReceived::ID,
                    {R"(<Request successFrom="3" type="D4" size="1" />)", Peers()[0].mac, ""});
      auto [showRequest, showReqId] = proxy->PopNextCommand();
      ASSERT_TRUE(showRequest);
      EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
      EXPECT_EQ(4U, showRequest->GetArgsCount());
      EXPECT_STREQ("D4", showRequest->GetArgAt(0).data());
      EXPECT_STREQ("1", showRequest->GetArgAt(1).data());
      EXPECT_STREQ("3", showRequest->GetArgAt(2).data());
      EXPECT_STREQ(Peers()[0].name.c_str(), showRequest->GetArgAt(3).data());
      RespondOK(showReqId);
      EXPECT_TRUE(proxy->NoCommands());

      ctrl->OnEvent(event::MessageReceived::ID,
                    {R"(<Response successCount="0" size="1" type="D4"><Val>2</Val></Response>)",
                     Peers()[1].mac,
                     ""});
      auto [showResponse, showRespId] = proxy->PopNextCommand();
      ASSERT_TRUE(showResponse);
      EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
      EXPECT_EQ(4u, showResponse->GetArgsCount());
      EXPECT_STREQ("2;", showResponse->GetArgAt(0).data());
      EXPECT_STREQ("D4", showResponse->GetArgAt(1).data());
      EXPECT_STREQ("0", showResponse->GetArgAt(2).data());
      EXPECT_STREQ(Peers()[1].name.c_str(), showResponse->GetArgAt(3).data());
      RespondOK(showRespId);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // one response from unauthorized peer
   ctrl->OnEvent(event::MessageReceived::ID,
                 {R"(<Response successCount="1" size="1" type="D4"><Val>4</Val></Response>)",
                  Peers()[0].mac,
                  ""});
   EXPECT_TRUE(proxy->NoCommands());

   // one authorized response without previous request
   ctrl->OnEvent(event::MessageReceived::ID,
                 {R"(<Response successCount="1" size="1" type="D6"><Val>5</Val></Response>)",
                  Peers()[1].mac,
                  ""});
   auto [showResponse, showRespId] = proxy->PopNextCommand();
   ASSERT_TRUE(showResponse);
   EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
   EXPECT_EQ(4u, showResponse->GetArgsCount());
   EXPECT_STREQ("5;", showResponse->GetArgAt(0).data());
   EXPECT_STREQ("D6", showResponse->GetArgAt(1).data());
   EXPECT_STREQ("1", showResponse->GetArgAt(2).data());
   EXPECT_STREQ(Peers()[1].name.c_str(), showResponse->GetArgAt(3).data());
   RespondOK(showRespId);

   // starting negotiation
   timer->FastForwardTime();
   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());
   auto [negotiationStart, negStartId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   RespondOK(negStartId);

   std::string offer = R"(<Offer round="14"><Mac>)" + localMac + "</Mac></Offer>";
   for (auto it = std::crbegin(Peers()); it != std::crend(Peers()); ++it) {
      auto [sendOffer, id] = proxy->PopNextCommand();
      ASSERT_TRUE(sendOffer);
      EXPECT_EQ(cmd::SendMessage::ID, sendOffer->GetId());
      EXPECT_EQ(2U, sendOffer->GetArgsCount());
      EXPECT_STREQ(offer.c_str(), sendOffer->GetArgAt(0).data());
      EXPECT_STREQ(it->mac.c_str(), sendOffer->GetArgAt(1).data());
      RespondOK(id);
   }
   EXPECT_TRUE(proxy->NoCommands());
}

using P2R15 = PlayingFixture<2u, 15u>;

TEST_F(P2R15, renegotiates_when_generator_doesnt_answer_requests)
{
   ctrl->OnEvent(event::CastRequestIssued::ID, {"D4", "1", "3"});
   auto [showRequest, showReqId] = proxy->PopNextCommand();
   ASSERT_TRUE(showRequest);
   EXPECT_EQ(cmd::ShowRequest::ID, showRequest->GetId());
   RespondOK(showReqId);

   // sends request
   const auto expectedRequest = dice::Request{dice::MakeCast("D4", 1), 3u};
   for (const auto & peer : Peers()) {
      auto [sendRequest, id] = proxy->PopNextCommand();
      ASSERT_TRUE(sendRequest);
      EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
      EXPECT_EQ(2U, sendRequest->GetArgsCount());

      const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
      EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
      EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

      EXPECT_STREQ(peer.mac.c_str(), sendRequest->GetArgAt(1).data());
      RespondOK(id);
   }
   EXPECT_TRUE(proxy->NoCommands());

   // received response to other request
   ctrl->OnEvent(event::MessageReceived::ID,
                 {R"(<Response successCount="1" size="1" type="D6"><Val>5</Val></Response>)",
                  Peers()[0].mac,
                  ""});
   auto [showResponse, showRespId] = proxy->PopNextCommand();
   ASSERT_TRUE(showResponse);
   EXPECT_EQ(cmd::ShowResponse::ID, showResponse->GetId());
   RespondOK(showRespId);

   // retries request
   for (int i = 0; i < 2; ++i) {
      timer->FastForwardTime(1s);
      auto [sendRequest, id] = proxy->PopNextCommand();
      ASSERT_TRUE(sendRequest);
      EXPECT_EQ(cmd::SendMessage::ID, sendRequest->GetId());
      EXPECT_EQ(2U, sendRequest->GetArgsCount());

      const auto actualRequest = serializer->Deserialize(sendRequest->GetArgAt(0));
      EXPECT_TRUE(std::holds_alternative<dice::Request>(actualRequest));
      EXPECT_EQ(expectedRequest, std::get<dice::Request>(actualRequest));

      EXPECT_STREQ(Peers()[0].mac.c_str(), sendRequest->GetArgAt(1).data());
      RespondOK(id);
      EXPECT_TRUE(proxy->NoCommands());
   }

   // no success => goes to Negotiation
   timer->FastForwardTime(1s);
   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());
   auto [negotiationStart, negStartId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   RespondOK(negStartId);

   std::string offer = R"(<Offer round="16"><Mac>)" + Peers()[1].mac + "</Mac></Offer>";
   for (auto it = std::crbegin(Peers()); it != std::crend(Peers()); ++it) {
      auto [sendOffer, id] = proxy->PopNextCommand();
      ASSERT_TRUE(sendOffer);
      EXPECT_EQ(cmd::SendMessage::ID, sendOffer->GetId());
      EXPECT_EQ(2U, sendOffer->GetArgsCount());
      EXPECT_STREQ(offer.c_str(), sendOffer->GetArgAt(0).data());
      EXPECT_STREQ(it->mac.c_str(), sendOffer->GetArgAt(1).data());
      RespondOK(id);
   }
   EXPECT_TRUE(proxy->NoCommands());
}

using P2R17 = PlayingFixture<2u, 17u>;

TEST_F(P2R17, disconnects_peers_that_are_in_error_state_at_the_end)
{
   timer->FastForwardTime(10s);

   // both peers report read errors...
   ctrl->OnEvent(event::SocketReadFailed::ID, {Peers()[0].mac, ""});
   EXPECT_TRUE(proxy->NoCommands());
   ctrl->OnEvent(event::SocketReadFailed::ID, {Peers()[1].mac, ""});
   EXPECT_TRUE(proxy->NoCommands());
   timer->FastForwardTime(1s);
   EXPECT_TRUE(proxy->NoCommands());

   // ...but peer 0 comes back with an offer
   std::string offer = R"(<Offer round="19"><Mac>)" + Peers()[1].mac + "</Mac></Offer>";
   ctrl->OnEvent(event::MessageReceived::ID, {offer, Peers()[0].mac, ""});

   // so we should disconnect peer 1 and start negotiation...
   auto [disconnect, disconnectId] = proxy->PopNextCommand();
   ASSERT_TRUE(disconnect);
   EXPECT_EQ(cmd::CloseConnection::ID, disconnect->GetId());
   EXPECT_EQ(2U, disconnect->GetArgsCount());
   EXPECT_STREQ(Peers()[1].mac.c_str(), disconnect->GetArgAt(1).data());

   timer->FastForwardTime();
   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());
   auto [negotiationStart, negStartId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());

   EXPECT_TRUE(proxy->NoCommands());
   RespondOK(disconnectId);
   RespondOK(negStartId);

   // ...with only one remote peer
   std::string expectedOffer = R"(<Offer round="19"><Mac>)" + localMac + "</Mac></Offer>";
   auto [sendOffer, id] = proxy->PopNextCommand();
   ASSERT_TRUE(sendOffer);
   EXPECT_EQ(cmd::SendMessage::ID, sendOffer->GetId());
   EXPECT_EQ(2U, sendOffer->GetArgsCount());
   EXPECT_STREQ(expectedOffer.c_str(), sendOffer->GetArgAt(0).data());
   EXPECT_STREQ(Peers()[0].mac.c_str(), sendOffer->GetArgAt(1).data());
   RespondOK(id);

   EXPECT_TRUE(proxy->NoCommands());
}

using P2R20 = PlayingFixture<2u, 20u>;

TEST_F(P2R20, resets_and_goes_to_idle_on_game_stop)
{
   size_t prevBlockCount = cmd::pool.GetBlockCount();
   ctrl->OnEvent(event::GameStopped::ID, {}); // game stopped

   {
      auto [reset, id] = proxy->PopNextCommand();
      ASSERT_TRUE(reset);
      EXPECT_EQ(cmd::ResetConnections::ID, reset->GetId());
      EXPECT_EQ(0U, reset->GetArgsCount());
      RespondOK(id);
   }
   {
      auto [reset, id] = proxy->PopNextCommand();
      ASSERT_TRUE(reset);
      EXPECT_EQ(cmd::ResetGame::ID, reset->GetId());
      EXPECT_EQ(0U, reset->GetArgsCount());
      RespondOK(id);
   }

   timer->FastForwardTime();
   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());
   EXPECT_TRUE(cmd::pool.GetBlockCount() <= prevBlockCount);
   auto [btOn, id] = proxy->PopNextCommand();
   ASSERT_TRUE(btOn);
   EXPECT_EQ(cmd::EnableBluetooth::ID, btOn->GetId());
   EXPECT_EQ(0U, btOn->GetArgsCount());
   RespondOK(id);

   EXPECT_TRUE(proxy->NoCommands());
}

using P2R21 = PlayingFixture<2u, 21u>;

TEST_F(P2R21, goes_to_idle_from_mid_game_negotiation_if_game_stopped)
{
   timer->FastForwardTime(10s);
   {
      std::string offer = R"(<Offer round="19"><Mac>)" + Peers()[1].mac + "</Mac></Offer>";
      ctrl->OnEvent(event::MessageReceived::ID, {offer, Peers()[0].mac, ""});
   }

   timer->FastForwardTime();
   auto [negotiationStart, negStartId] = proxy->PopNextCommand();
   ASSERT_TRUE(negotiationStart);
   EXPECT_EQ(cmd::NegotiationStart::ID, negotiationStart->GetId());
   RespondOK(negStartId);

   EXPECT_EQ("New state: StateNegotiating", logger.GetLastStateLine());
   for (size_t i = 0; i < Peers().size(); ++i) {
      auto [offer, id] = proxy->PopNextCommand();
      ASSERT_TRUE(offer);
      EXPECT_EQ(cmd::SendMessage::ID, offer->GetId());
      RespondOK(id);
   }

   ctrl->OnEvent(event::GameStopped::ID, {});

   auto resetBt = proxy->PopNextCommand().c;
   ASSERT_TRUE(resetBt);
   EXPECT_EQ(cmd::ResetConnections::ID, resetBt->GetId());
   EXPECT_EQ(0U, resetBt->GetArgsCount());

   timer->FastForwardTime();
   EXPECT_EQ("New state: StateIdle", logger.GetLastStateLine());
}

// round 23

} // namespace