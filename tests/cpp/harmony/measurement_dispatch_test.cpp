#include "a2ui/utils/a2ui_measurement_dispatch.h"

#include <gtest/gtest.h>

#include <condition_variable>
#include <thread>

namespace {

struct MeasurementRequest {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    int result = 0;
};

}

TEST(HarmonyMeasurementDispatchTest, FailedSubmissionDoesNotWaitForCompletion) {
    MeasurementRequest request;
    const bool dispatched = a2ui::dispatchMeasurementAndWait(request, [](MeasurementRequest*) {
        return false;
    });
    EXPECT_FALSE(dispatched);
    EXPECT_FALSE(request.done);
    EXPECT_EQ(request.result, 0);
}

TEST(HarmonyMeasurementDispatchTest, CompletionBeforeWaitingIsNotLost) {
    MeasurementRequest request;
    const bool dispatched = a2ui::dispatchMeasurementAndWait(request, [](MeasurementRequest* pending) {
        pending->result = 48;
        a2ui::completeMeasurement(*pending);
        return true;
    });
    EXPECT_TRUE(dispatched);
    EXPECT_TRUE(request.done);
    EXPECT_EQ(request.result, 48);
}

TEST(HarmonyMeasurementDispatchTest, AsynchronousCompletionReturnsTheResult) {
    MeasurementRequest request;
    std::thread callback;
    const bool dispatched = a2ui::dispatchMeasurementAndWait(request, [&callback](MeasurementRequest* pending) {
        callback = std::thread([pending] {
            pending->result = 96;
            a2ui::completeMeasurement(*pending);
        });
        return true;
    });
    callback.join();
    EXPECT_TRUE(dispatched);
    EXPECT_EQ(request.result, 96);
}

TEST(HarmonyMeasurementDispatchTest, CompletionWithoutJavascriptReturnsZero) {
    MeasurementRequest request;
    const bool dispatched = a2ui::dispatchMeasurementAndWait(request, [](MeasurementRequest* pending) {
        a2ui::completeMeasurement(*pending);
        return true;
    });
    EXPECT_TRUE(dispatched);
    EXPECT_EQ(request.result, 0);
}
