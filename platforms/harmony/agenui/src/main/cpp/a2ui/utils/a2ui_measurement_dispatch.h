#pragma once

#include <mutex>

namespace a2ui {

template <typename Request>
void completeMeasurement(Request& request) {
    std::lock_guard<std::mutex> lock(request.mtx);
    request.done = true;
    request.cv.notify_one();
}

template <typename Request, typename Dispatch>
bool dispatchMeasurementAndWait(Request& request, Dispatch dispatch) {
    if (!dispatch(&request)) {
        return false;
    }
    std::unique_lock<std::mutex> lock(request.mtx);
    request.cv.wait(lock, [&request] { return request.done; });
    return true;
}

}
