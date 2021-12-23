#include <gtest/gtest.h>

#include "ctrl/timer.hpp"
#include "utils/task.hpp"

namespace {
using namespace std::chrono_literals;
using core::Timer;

TEST(TimerTest, timer_schedules_delayed_task_correctly)
{
   std::function<void()> pendingTask;
   std::chrono::milliseconds requestedDelay;
   bool taskFinished = false;

   Timer timer([&](auto task, std::chrono::milliseconds delay) {
      pendingTask = std::move(task);
      requestedDelay = delay;
   });

   auto StartTimer = [&]() -> cr::DetachedHandle {
      co_await timer.WaitFor(3s);
      taskFinished = true;
   };

   StartTimer();
   EXPECT_FALSE(taskFinished);
   EXPECT_EQ(3s, requestedDelay);
   EXPECT_TRUE(pendingTask);

   pendingTask();
   EXPECT_TRUE(taskFinished);
}

TEST(TimerTest, timer_schedules_immediate_task)
{
   std::function<void()> pendingTask;
   std::chrono::milliseconds requestedDelay = 123ms;
   bool task1Finished = false;
   bool task2Finished = false;

   Timer timer([&](auto task, std::chrono::milliseconds delay) {
      pendingTask = std::move(task);
      requestedDelay = delay;
   });

   auto StartTimer = [&timer](bool & finished,
                              std::chrono::milliseconds timeout) -> cr::DetachedHandle {
      co_await timer.WaitFor(timeout);
      finished = true;
   };

   StartTimer(task1Finished, 0s);
   EXPECT_FALSE(task1Finished);
   EXPECT_EQ(0ms, requestedDelay);
   EXPECT_TRUE(pendingTask);

   pendingTask();
   EXPECT_TRUE(task1Finished);

   StartTimer(task2Finished, -42s);
   EXPECT_FALSE(task2Finished);
   EXPECT_EQ(0ms, requestedDelay);
   EXPECT_TRUE(pendingTask);

   pendingTask();
   EXPECT_TRUE(task2Finished);
}

} // namespace
