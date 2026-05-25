// CT*: Thread-safety tests for engine + SurfaceManager APIs.
//
// These tests don't make functional assertions — they're structured to
// trigger races that ASan/TSan would catch. With Sanitizer disabled they
// just verify "no crash, no deadlock".

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_surface_manager_interface.h"
#include "support/mock_message_listener.h"
#include "support/mock_platform_function.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

constexpr int kSpawnedThreads = 4;
constexpr int kIterations = 50;

// CT001
TEST(ConcurrencyTest, CT001_AddRemoveListener_FromMultipleThreads) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);

    std::vector<std::thread> threads;
    std::vector<::agenui::testing::MockMessageListener> listeners(
        kSpawnedThreads);

    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int k = 0; k < kIterations; ++k) {
                sm->addSurfaceEventListener(&listeners[i]);
                sm->removeSurfaceEventListener(&listeners[i]);
            }
        });
    }
    for (auto& t : threads) t.join();
    ::agenui::testing::WaitForWorkerIdle();
}

// CT002
TEST(ConcurrencyTest, CT002_ReceiveTextChunk_FromMultipleThreads) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);

    std::vector<std::thread> threads;
    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int k = 0; k < 30; ++k) {
                sm->beginTextStream();
                sm->receiveTextChunk("partial");
                sm->endTextStream();
            }
        });
    }
    for (auto& t : threads) t.join();
    ::agenui::testing::WaitForWorkerIdle();
}

// CT003
TEST(ConcurrencyTest, CT003_SubmitUIAction_FromMultipleThreads) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);

    std::vector<std::thread> threads;
    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&]() {
            for (int k = 0; k < 30; ++k) {
                ::agenui::ActionMessage msg;
                msg.surfaceId = "ct003";
                msg.sourceComponentId = "x";
                sm->submitUIAction(msg);
            }
        });
    }
    for (auto& t : threads) t.join();
    ::agenui::testing::WaitForWorkerIdle();
}

// CT004
TEST(ConcurrencyTest, CT004_RegisterUnregisterFunction_Concurrent) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    std::vector<std::thread> threads;
    std::vector<::agenui::testing::MockPlatformFunction> functions(
        kSpawnedThreads);

    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&, i]() {
            std::string name = "ct004_fn_" + std::to_string(i);
            for (int k = 0; k < 20; ++k) {
                std::string config = R"({"name":")" + name + R"("})";
                engine->registerFunction(config, &functions[i]);
                engine->unregisterFunction(name);
            }
        });
    }
    for (auto& t : threads) t.join();
}

// CT005
TEST(ConcurrencyTest, CT005_LoadThemeConfig_Concurrent) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    std::vector<std::thread> threads;
    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&]() {
            for (int k = 0; k < 20; ++k) {
                std::string err;
                engine->loadThemeConfig("{}", err);
            }
        });
    }
    for (auto& t : threads) t.join();
}

// CT006
TEST(ConcurrencyTest, CT006_SetDayNightMode_Concurrent) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    std::vector<std::thread> threads;
    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int k = 0; k < 20; ++k) {
                engine->setDayNightMode(i % 2 == 0 ? "light" : "dark");
            }
        });
    }
    for (auto& t : threads) t.join();
    ::agenui::testing::WaitForWorkerIdle();
}

// CT008: MeasurementManager concurrent register / measure / unregister.
TEST(ConcurrencyTest, CT008_MeasurementManager_RegisterMeasure_Concurrent) {
    auto* engine = ::agenui::testing::GetEngine();
    auto* mgr = engine->getMeasurementManager();
    ASSERT_NE(mgr, nullptr);

    class Trivial : public ::agenui::IMeasurement {
    public:
        ::agenui::MeasureResult measure(const std::string&,
                                        const ::agenui::MeasureModes&) override {
            return {};
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kSpawnedThreads; ++i) {
        threads.emplace_back([&, i]() {
            std::string type = "CT008_T" + std::to_string(i);
            auto impl = std::make_shared<Trivial>();
            for (int k = 0; k < 30; ++k) {
                mgr->registerMeasurement(type, impl);
                ::agenui::MeasureModes modes;
                (void)mgr->measure(type, "{}", modes);
                mgr->unregisterMeasurement(type);
            }
        });
    }
    for (auto& t : threads) t.join();
}

}  // namespace
