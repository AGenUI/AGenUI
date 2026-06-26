#include <gtest/gtest.h>
#include "module/agenui_message_thread.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <future>

using namespace agenui;

// ═══════════════════════════════════════════════════════════════════════════════════
// MessageThread Unit Tests
// ═══════════════════════════════════════════════════════════════════════════════════

class MessageThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        thread = std::make_unique<MessageThread>("TestThread");
    }

    void TearDown() override {
        if (thread && thread->isRunning()) {
            thread->stop();
        }
    }

    std::unique_ptr<MessageThread> thread;
};

// --- Lifecycle Tests ---

TEST_F(MessageThreadTest, Construction_NotRunning) {
    EXPECT_FALSE(thread->isRunning());
}

TEST_F(MessageThreadTest, Start_ReturnsTrue) {
    EXPECT_TRUE(thread->start());
}

TEST_F(MessageThreadTest, Start_IsRunning) {
    thread->start();
    EXPECT_TRUE(thread->isRunning());
}

TEST_F(MessageThreadTest, Stop_NotRunning) {
    thread->start();
    thread->stop();
    EXPECT_FALSE(thread->isRunning());
}

TEST_F(MessageThreadTest, DoubleStart_IdempotentOrFails) {
    EXPECT_TRUE(thread->start());
    // Second start should either return true (idempotent) or false
    // but must not crash
    thread->start();
    EXPECT_TRUE(thread->isRunning());
}

TEST_F(MessageThreadTest, StopWithoutStart_NoCrash) {
    // Should not crash
    thread->stop();
    EXPECT_FALSE(thread->isRunning());
}

// --- Post Task Tests ---

TEST_F(MessageThreadTest, Post_TaskExecuted) {
    thread->start();
    std::promise<bool> promise;
    auto future = promise.get_future();

    thread->post([&promise]() {
        promise.set_value(true);
    });

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(future.get());
}

TEST_F(MessageThreadTest, Post_MultipleTasks_ExecutedInOrder) {
    thread->start();
    std::vector<int> order;
    std::mutex orderMutex;
    std::promise<void> done;
    auto future = done.get_future();

    for (int i = 0; i < 5; ++i) {
        thread->post([&order, &orderMutex, &done, i]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(i);
            if (i == 4) {
                done.set_value();
            }
        });
    }

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    std::lock_guard<std::mutex> lock(orderMutex);
    ASSERT_EQ(order.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(order[i], i);
    }
}

TEST_F(MessageThreadTest, Post_ManyTasks_AllExecuted) {
    thread->start();
    std::atomic<int> counter{0};
    std::promise<void> done;
    auto future = done.get_future();
    const int taskCount = 100;

    for (int i = 0; i < taskCount; ++i) {
        thread->post([&counter, &done, taskCount]() {
            if (++counter == taskCount) {
                done.set_value();
            }
        });
    }

    auto status = future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_EQ(counter.load(), taskCount);
}

// --- PostDelayed Tests ---

TEST_F(MessageThreadTest, PostDelayed_TaskExecutedAfterDelay) {
    thread->start();
    auto startTime = std::chrono::steady_clock::now();
    std::promise<std::chrono::steady_clock::time_point> promise;
    auto future = promise.get_future();

    thread->postDelayed([&promise]() {
        promise.set_value(std::chrono::steady_clock::now());
    }, 50);  // 50ms delay

    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    auto executeTime = future.get();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(executeTime - startTime).count();
    // Should execute after at least ~40ms (allowing some timing slack)
    EXPECT_GE(elapsed, 30);
}

TEST_F(MessageThreadTest, PostDelayed_MultipleDelays_OrderedByTime) {
    thread->start();
    std::vector<int> order;
    std::mutex orderMutex;
    std::promise<void> done;
    auto future = done.get_future();

    // Post in reverse delay order — execution should still be ordered by delay
    thread->postDelayed([&order, &orderMutex, &done]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(3);
        if (order.size() == 3) done.set_value();
    }, 150);

    thread->postDelayed([&order, &orderMutex, &done]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(1);
        if (order.size() == 3) done.set_value();
    }, 30);

    thread->postDelayed([&order, &orderMutex, &done]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(2);
        if (order.size() == 3) done.set_value();
    }, 80);

    auto status = future.wait_for(std::chrono::seconds(3));
    ASSERT_EQ(status, std::future_status::ready);

    std::lock_guard<std::mutex> lock(orderMutex);
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// --- Thread ID ---

TEST_F(MessageThreadTest, GetThreadId_AfterStart_NotDefault) {
    thread->start();
    // Give thread time to start loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto tid = thread->getThreadId();
    // Thread ID should be valid (not default-constructed)
    EXPECT_NE(tid, std::thread::id{});
}

TEST_F(MessageThreadTest, GetThreadId_DifferentFromCurrent) {
    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto tid = thread->getThreadId();
    EXPECT_NE(tid, std::this_thread::get_id());
}

// --- Edge Cases ---

TEST_F(MessageThreadTest, PostAfterStop_NoCrash) {
    thread->start();
    thread->stop();
    EXPECT_NO_THROW(thread->post([]() {}));
    EXPECT_FALSE(thread->isRunning());
}

TEST_F(MessageThreadTest, PostBeforeStart_NoCrash) {
    EXPECT_NO_THROW(thread->post([]() {}));
    EXPECT_FALSE(thread->isRunning());
}

TEST_F(MessageThreadTest, ConcurrentPost_FromMultipleThreads_AllExecuted) {
    thread->start();
    std::atomic<int> counter{0};
    std::promise<void> done;
    auto future = done.get_future();
    const int threadCount = 5;
    const int tasksPerThread = 20;
    const int totalTasks = threadCount * tasksPerThread;
    std::atomic<bool> doneSet{false};

    std::vector<std::thread> posters;
    posters.reserve(threadCount);
    for (int t = 0; t < threadCount; ++t) {
        posters.emplace_back([&]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                thread->post([&]() {
                    if (++counter == totalTasks) {
                        bool expected = false;
                        if (doneSet.compare_exchange_strong(expected, true)) {
                            done.set_value();
                        }
                    }
                });
            }
        });
    }
    for (auto& p : posters) p.join();

    auto status = future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_EQ(counter.load(), totalTasks);
}

TEST_F(MessageThreadTest, StartStopStart_SecondStartWorks) {
    EXPECT_TRUE(thread->start());
    thread->stop();
    EXPECT_FALSE(thread->isRunning());
    EXPECT_TRUE(thread->start());
    EXPECT_TRUE(thread->isRunning());

    std::promise<bool> promise;
    auto future = promise.get_future();
    thread->post([&promise]() { promise.set_value(true); });
    auto status = future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(future.get());
}

TEST_F(MessageThreadTest, StopWhileTasksPending_GracefulShutdown) {
    thread->start();
    std::atomic<int> executed{0};

    for (int i = 0; i < 10; ++i) {
        thread->post([&executed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            executed++;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    thread->stop();
    EXPECT_FALSE(thread->isRunning());
    EXPECT_GT(executed.load(), 0);
}
