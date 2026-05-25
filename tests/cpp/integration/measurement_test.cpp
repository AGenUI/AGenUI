// MS*: IMeasurementManager tests.

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_measurement.h"
#include "support/test_env.h"

namespace {

using ::agenui::CalcType;
using ::agenui::IMeasurement;
using ::agenui::IMeasurementManager;
using ::agenui::MeasureModes;
using ::agenui::MeasureResult;

class FixedMeasurement : public IMeasurement {
public:
    MeasureResult measure(const std::string&, const MeasureModes&) override {
        MeasureResult r;
        r.calcType = CalcType::Sync;
        r.width = 42.0f;
        r.height = 24.0f;
        r.countOfLines = 1;
        return r;
    }
};

class AsyncMeasurement : public IMeasurement {
public:
    MeasureResult measure(const std::string&, const MeasureModes&) override {
        MeasureResult r;
        r.calcType = CalcType::Async;
        return r;
    }
};

class MeasurementTest : public ::testing::Test {
protected:
    IMeasurementManager* mgr_ = nullptr;
    void SetUp() override {
        auto* engine = ::agenui::testing::GetEngine();
        ASSERT_NE(engine, nullptr);
        mgr_ = engine->getMeasurementManager();
        ASSERT_NE(mgr_, nullptr);
    }
};

// MS001
TEST_F(MeasurementTest, MS001_RegisterMeasurement_TypeAvailable) {
    auto impl = std::make_shared<FixedMeasurement>();
    mgr_->registerMeasurement("MS001_Type", impl);
    auto out = mgr_->getMeasurement("MS001_Type");
    EXPECT_NE(out, nullptr);
    mgr_->unregisterMeasurement("MS001_Type");
}

// MS002
TEST_F(MeasurementTest, MS002_UnregisterMeasurement_TypeRemoved) {
    auto impl = std::make_shared<FixedMeasurement>();
    mgr_->registerMeasurement("MS002_Type", impl);
    mgr_->unregisterMeasurement("MS002_Type");
    EXPECT_EQ(mgr_->getMeasurement("MS002_Type"), nullptr);
}

// MS003
TEST_F(MeasurementTest, MS003_Measure_KnownType_ReturnsRealSize) {
    auto impl = std::make_shared<FixedMeasurement>();
    mgr_->registerMeasurement("MS003_Type", impl);
    MeasureModes modes;
    auto r = mgr_->measure("MS003_Type", "{}", modes);
    EXPECT_FLOAT_EQ(r.width, 42.0f);
    EXPECT_FLOAT_EQ(r.height, 24.0f);
    mgr_->unregisterMeasurement("MS003_Type");
}

// MS004
TEST_F(MeasurementTest, MS004_Measure_UnknownType_ReturnsZero) {
    MeasureModes modes;
    auto r = mgr_->measure("MS004_Unknown", "{}", modes);
    EXPECT_FLOAT_EQ(r.width, 0.0f);
    EXPECT_FLOAT_EQ(r.height, 0.0f);
}

// MS005
TEST_F(MeasurementTest, MS005_AsyncImplementation_ReturnsAsyncFlag) {
    auto impl = std::make_shared<AsyncMeasurement>();
    mgr_->registerMeasurement("MS005_Type", impl);
    MeasureModes modes;
    auto r = mgr_->measure("MS005_Type", "{}", modes);
    EXPECT_EQ(r.calcType, CalcType::Async);
    EXPECT_FLOAT_EQ(r.width, 0.0f);
    EXPECT_FLOAT_EQ(r.height, 0.0f);
    mgr_->unregisterMeasurement("MS005_Type");
}

// MS006: getMeasurement returns nullptr for unknown.
TEST_F(MeasurementTest, MS006_GetMeasurement_Unknown_ReturnsNull) {
    EXPECT_EQ(mgr_->getMeasurement("MS006_Nope"), nullptr);
}

// MS007: register / unregister concurrently from multiple threads.
TEST_F(MeasurementTest, MS007_Concurrent_RegisterMeasure_NoCrash) {
    constexpr int kThreads = 4;
    constexpr int kIters = 50;
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([this, t, &failures]() {
            for (int i = 0; i < kIters; ++i) {
                std::string type = "MS007_" + std::to_string(t) + "_" +
                                   std::to_string(i);
                auto impl = std::make_shared<FixedMeasurement>();
                mgr_->registerMeasurement(type, impl);
                MeasureModes modes;
                auto r = mgr_->measure(type, "{}", modes);
                if (r.width != 42.0f) ++failures;
                mgr_->unregisterMeasurement(type);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}

// MS008: unregister while concurrent measure is running. We can't strictly
// observe this without instrumentation; smoke-check only.
TEST_F(MeasurementTest, MS008_UnregisterDuringMeasure_NoCrash) {
    auto impl = std::make_shared<FixedMeasurement>();
    mgr_->registerMeasurement("MS008_Type", impl);

    std::atomic<bool> stop{false};
    std::thread reader([this, &stop]() {
        MeasureModes modes;
        while (!stop.load()) {
            (void)mgr_->measure("MS008_Type", "{}", modes);
        }
    });
    for (int i = 0; i < 50; ++i) {
        mgr_->unregisterMeasurement("MS008_Type");
        mgr_->registerMeasurement("MS008_Type", impl);
    }
    stop.store(true);
    reader.join();
    mgr_->unregisterMeasurement("MS008_Type");
}

}  // namespace
