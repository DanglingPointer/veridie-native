#include <gtest/gtest.h>
#include "utils/task.hpp"
#include "sign/commandmanager.hpp"
#include "sign/commands.hpp"
#include "sign/commandpool.hpp"
#include "dice/serializer.hpp"
#include "fakelogger.hpp"

namespace {
using namespace cmd;

class CmdFixture : public ::testing::Test
{
protected:
   CmdFixture() = default;

   FakeLogger logger;
};


// TEST_F(CmdFixture, response_code_subset_maps_values_correctly)
//{
//   StopListeningResponse r1(ResponseCode::OK::value);
//   r1.Handle(
//      [](ResponseCode::OK) {
//         // OK
//      },
//      [](ResponseCode::INVALID_STATE) {
//         ADD_FAILURE();
//      });
//   EXPECT_EQ(ResponseCode::OK::value, r1.Value());
//
//   StopListeningResponse r2(ResponseCode::INVALID_STATE::value);
//   r2.Handle(
//      [](ResponseCode::OK) {
//         ADD_FAILURE();
//      },
//      [](ResponseCode::INVALID_STATE) {
//         // OK
//      });
//   EXPECT_EQ(ResponseCode::INVALID_STATE::value, r2.Value());
//
//   EXPECT_THROW({ StopListeningResponse r4(42); }, std::invalid_argument);
//}

TEST_F(CmdFixture, common_base_stores_arguments_correctly)
{
   auto cast = dice::MakeCast("D6", 4);

   const std::string player1 = "Player 1";

   ShowResponse cmd(cast, "D100", 2, player1);

   EXPECT_EQ(ShowResponse::ID, cmd.GetId());
   EXPECT_STREQ("ShowResponse", cmd.GetName().data());
   EXPECT_EQ(4U, cmd.GetArgsCount());
   EXPECT_STREQ("0;0;0;0;", cmd.GetArgAt(0).data());
   EXPECT_STREQ("D100", cmd.GetArgAt(1).data());
   EXPECT_STREQ("2", cmd.GetArgAt(2).data());
   EXPECT_STREQ("Player 1", cmd.GetArgAt(3).data());
}

// TEST_F(CmdFixture, invalid_response_throws_exception)
//{
//   NegotiationStart cmd(MakeCb([&](NegotiationStartResponse r) {
//      ADD_FAILURE() << "Responded to: " << r.Value();
//   }));
//   EXPECT_EQ(NegotiationStart::ID, cmd.GetId());
//   EXPECT_STREQ("NegotiationStart", cmd.GetName().data());
//   EXPECT_EQ(0U, cmd.GetArgsCount());
//
//   EXPECT_THROW(cmd.Respond(123456789), std::invalid_argument);
//}

struct TestCommand : ICommand
{
   template <typename... Args>
   TestCommand(int32_t id, Args &&... args)
      : id(id)
      , args({std::forward<Args>(args)...})
   {}

   int32_t GetId() const override { return id; }
   std::string_view GetName() const override { return "TestCommand"; }
   size_t GetArgsCount() const override { return args.size(); }
   std::string_view GetArgAt(size_t index) const override { return args[index]; }

   const int32_t id;
   const std::vector<std::string> args;
};

struct MockExternalInvoker : IExternalInvoker
{
   bool Invoke(mem::pool_ptr<cmd::ICommand> && data, int32_t id) override
   {
      receivedCommands.emplace_back(std::move(data), id);
      return !fail;
   }

   struct Invocation
   {
      Invocation(mem::pool_ptr<cmd::ICommand> && cmd, int32_t id)
         : cmd(std::move(cmd))
         , id(id)
      {}
      Invocation(Invocation && rhs) noexcept = default;
      mem::pool_ptr<cmd::ICommand> cmd;
      int32_t id;
   };
   std::vector<Invocation> receivedCommands;
   bool fail = false;
};

class ManagerFixture : public ::testing::Test
{
public:
   ManagerFixture()
   {
      auto uiMockInvoker = std::make_unique<MockExternalInvoker>();
      uiInvoker = uiMockInvoker.get();
      auto btMockInvoker = std::make_unique<MockExternalInvoker>();
      btInvoker = btMockInvoker.get();

      manager = std::make_unique<Manager>(std::move(uiMockInvoker), std::move(btMockInvoker));
   }

   FakeLogger logger;
   std::unique_ptr<Manager> manager;
   MockExternalInvoker * uiInvoker;
   MockExternalInvoker * btInvoker;
};

template <typename F>
cr::DetachedHandle coawait_and_get_response(F && f, int64_t * outResponse)
{
   auto response = co_await f();
   if (outResponse)
      *outResponse = response;
}

TEST_F(ManagerFixture, cmd_manager_forwards_command_and_responses_correctly)
{
   auto sentCmd1 = pool.MakeUnique<TestCommand>(NegotiationStart::ID, "LETS", "NEGOTIATE");
   int64_t response1 = 0;
   coawait_and_get_response(
      [&] {
         return manager->IssueBtCommand(std::move(sentCmd1));
      },
      &response1);
   {
      EXPECT_EQ(1, btInvoker->receivedCommands.size());
      EXPECT_EQ(NegotiationStart::ID, btInvoker->receivedCommands[0].id);
      const auto * recvdCmd = btInvoker->receivedCommands[0].cmd.get();
      EXPECT_EQ(2, recvdCmd->GetArgsCount());
      EXPECT_STREQ("LETS", recvdCmd->GetArgAt(0).data());
      EXPECT_STREQ("NEGOTIATE", recvdCmd->GetArgAt(1).data());
   }
   EXPECT_EQ(0, response1);
   manager->SubmitResponse(btInvoker->receivedCommands[0].id, 42);
   EXPECT_EQ(42, response1);
}

TEST_F(ManagerFixture, cmd_manager_forwards_responses_out_of_order)
{
   auto sentCmd1 = pool.MakeUnique<TestCommand>(CloseConnection::ID);
   int64_t response1 = 0;
   coawait_and_get_response(
      [&] {
         return manager->IssueBtCommand(std::move(sentCmd1));
      },
      &response1);
   {
      EXPECT_EQ(1, btInvoker->receivedCommands.size());
      EXPECT_EQ(CloseConnection::ID, btInvoker->receivedCommands[0].id);
      const auto * recvdCmd = btInvoker->receivedCommands[0].cmd.get();
      EXPECT_EQ(0, recvdCmd->GetArgsCount());
   }


   auto sentCmd2 = pool.MakeUnique<TestCommand>(NegotiationStop::ID, "STOP", "NEGOTIATION");
   int64_t response2 = 0;
   coawait_and_get_response(
      [&] {
         return manager->IssueBtCommand(std::move(sentCmd2));
      },
      &response2);
   {
      EXPECT_EQ(2, btInvoker->receivedCommands.size());
      EXPECT_EQ(NegotiationStop::ID, btInvoker->receivedCommands[1].id);
      const auto * recvdCmd = btInvoker->receivedCommands[1].cmd.get();
      EXPECT_EQ(2, recvdCmd->GetArgsCount());
      EXPECT_STREQ("STOP", recvdCmd->GetArgAt(0).data());
      EXPECT_STREQ("NEGOTIATION", recvdCmd->GetArgAt(1).data());
   }


   auto sentCmd3 = pool.MakeUnique<TestCommand>(ShowToast::ID, "AWESOME TOAST");
   int64_t response3 = 0;
   coawait_and_get_response(
      [&] {
         return manager->IssueUiCommand(std::move(sentCmd3));
      },
      &response3);
   {
      EXPECT_EQ(1, uiInvoker->receivedCommands.size());
      EXPECT_EQ(ShowToast::ID, uiInvoker->receivedCommands[0].id);
      const auto * recvdCmd = uiInvoker->receivedCommands[0].cmd.get();
      EXPECT_EQ(1, recvdCmd->GetArgsCount());
      EXPECT_STREQ("AWESOME TOAST", recvdCmd->GetArgAt(0).data());
   }


   EXPECT_EQ(0, response1);
   EXPECT_EQ(0, response2);
   EXPECT_EQ(0, response3);

   manager->SubmitResponse(ShowToast::ID, 43);
   EXPECT_EQ(43, response3);

   manager->SubmitResponse(NegotiationStop::ID, 44);
   EXPECT_EQ(44, response2);

   manager->SubmitResponse(CloseConnection::ID, 45);
   EXPECT_EQ(45, response1);
}

TEST_F(ManagerFixture, cmd_manager_responds_to_pending_cmds_when_dying)
{
   auto sentCmd1 = pool.MakeUnique<TestCommand>(EnableBluetooth::ID);
   int64_t response1 = 0;

   auto sentCmd2 = pool.MakeUnique<TestCommand>(ShowRequest::ID);
   int64_t response2 = 0;

   auto sentCmd3 = pool.MakeUnique<TestCommand>(ShowToast::ID);

   coawait_and_get_response(
      [&] {
         return manager->IssueBtCommand(std::move(sentCmd1));
      },
      &response1);

   coawait_and_get_response(
      [&] {
         return manager->IssueUiCommand(std::move(sentCmd2));
      },
      &response2);

   manager->IssueUiCommand(std::move(sentCmd3));

   EXPECT_EQ(0, response1);
   EXPECT_EQ(0, response2);

   manager.reset();
   EXPECT_EQ(ICommand::INTEROP_FAILURE, response1);
   EXPECT_EQ(ICommand::INTEROP_FAILURE, response2);
}

TEST_F(ManagerFixture, cmd_manager_returns_error_on_overflow_immediately)
{
   std::vector<mem::pool_ptr<ICommand>> commands;
   for (int i = 0; i < (1 << 8); ++i)
      commands.emplace_back(pool.MakeUnique<TestCommand>(SendMessage::ID));

   for (size_t i = 0; i < commands.size(); ++i) {
      coawait_and_get_response(
         [&] {
            return manager->IssueUiCommand(std::move(commands[i]));
         },
         nullptr);
      EXPECT_EQ(i + 1, uiInvoker->receivedCommands.size());
      EXPECT_EQ(SendMessage::ID + i, uiInvoker->receivedCommands.back().id);
   }
   EXPECT_TRUE(logger.NoWarningsOrErrors());

   auto overflowCmd = pool.MakeUnique<TestCommand>(SendMessage::ID);
   int64_t overflowResponse = 0;
   coawait_and_get_response(
      [&] {
         return manager->IssueUiCommand(std::move(overflowCmd));
      },
      &overflowResponse);

   EXPECT_FALSE(logger.NoWarningsOrErrors());
   EXPECT_EQ(commands.size(), uiInvoker->receivedCommands.size());
   EXPECT_EQ(ICommand::INTEROP_FAILURE, overflowResponse);
   manager.reset();
}

TEST_F(ManagerFixture, cmd_manager_increments_id_for_non_awaited_commands)
{
   auto sentCmd1 = pool.MakeUnique<TestCommand>(EnableBluetooth::ID);
   auto sentCmd2 = pool.MakeUnique<TestCommand>(EnableBluetooth::ID);
   int64_t response2 = 0;

   manager->IssueUiCommand(std::move(sentCmd1));
   EXPECT_EQ(1, uiInvoker->receivedCommands.size());
   EXPECT_EQ(EnableBluetooth::ID, uiInvoker->receivedCommands.back().id);

   coawait_and_get_response(
      [&] {
         return manager->IssueUiCommand(std::move(sentCmd2));
      },
      &response2);

   EXPECT_EQ(2, uiInvoker->receivedCommands.size());
   EXPECT_EQ(EnableBluetooth::ID + 1, uiInvoker->receivedCommands.back().id);

   manager->SubmitResponse(EnableBluetooth::ID, 41);
   manager->SubmitResponse(EnableBluetooth::ID + 1, 42);
   EXPECT_EQ(42, response2);
}

TEST_F(ManagerFixture, cmd_manager_doesnt_increment_id_on_invoker_failure)
{
   auto sentCmd1 = pool.MakeUnique<TestCommand>(EnableBluetooth::ID);
   int64_t response1 = 0;
   auto sentCmd2 = pool.MakeUnique<TestCommand>(EnableBluetooth::ID);
   int64_t response2 = 0;

   uiInvoker->fail = true;
   coawait_and_get_response(
      [&] {
         return manager->IssueUiCommand(std::move(sentCmd1));
      },
      &response1);
   EXPECT_EQ(1, uiInvoker->receivedCommands.size());
   EXPECT_EQ(EnableBluetooth::ID, uiInvoker->receivedCommands.back().id);
   EXPECT_EQ(cmd::ICommand::INTEROP_FAILURE, response1);

   uiInvoker->fail = false;
   coawait_and_get_response(
      [&] {
         return manager->IssueUiCommand(std::move(sentCmd2));
      },
      &response2);

   EXPECT_EQ(2, uiInvoker->receivedCommands.size());
   EXPECT_EQ(EnableBluetooth::ID, uiInvoker->receivedCommands.back().id);

   manager->SubmitResponse(EnableBluetooth::ID, 42);
   EXPECT_EQ(42, response2);
}

} // namespace
