// Helpers for synchronizing with the engine's async processing.
//
// As a third-party black-box test module, we cannot access the engine's
// internal thread infrastructure (ThreadManager, MessageThread). Instead,
// we use public API-based synchronization: perform an observable operation
// via ISurfaceManager and wait for a callback confirmation.
//
// WaitForWorkerIdle creates a temporary SurfaceManager, sends an empty
// stream session, and waits for the onCreateSurface callback. Since the
// engine processes API calls on its worker thread in FIFO order, the
// callback fires only after all previously queued tasks have completed.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_message_listener.h"
#include "agenui_surface_manager_interface.h"

namespace agenui {
namespace testing {

// Internal helper: a lightweight listener that signals when any callback
// fires. Used by WaitForWorkerIdle to confirm the worker thread has
// processed all prior tasks.
class IdleSignalListener : public ::agenui::IAGenUIMessageListener {
public:
    void onCreateSurface(const ::agenui::CreateSurfaceMessage&) override {
        std::lock_guard<std::mutex> lk(mutex_);
        signaled_ = true;
        cv_.notify_all();
    }

    void onComponentsAdd(const std::string&,
                         const std::vector<::agenui::ComponentsAddMessage>&) override {
        std::lock_guard<std::mutex> lk(mutex_);
        signaled_ = true;
        cv_.notify_all();
    }

    void onError(const ::agenui::ErrorMessage&) override {
        std::lock_guard<std::mutex> lk(mutex_);
        signaled_ = true;
        cv_.notify_all();
    }

    bool waitForSignal(int timeoutMillis) {
        std::unique_lock<std::mutex> lk(mutex_);
        return cv_.wait_for(lk, std::chrono::milliseconds(timeoutMillis),
                            [&]() { return signaled_; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool signaled_ = false;
};

// Wait until the engine's worker thread has drained all previously posted
// tasks. Returns true on success, false on timeout.
//
// Implementation: creates a temporary SurfaceManager, adds a listener,
// sends an empty streaming session (beginTextStream + endTextStream),
// and waits for ANY callback to fire. Since the engine processes tasks
// in FIFO order on its worker thread, a callback can only fire after
// all previously queued work has completed.
//
// This is a black-box approach: we only use public IAGenUIEngine and
// ISurfaceManager interfaces, never touching internal thread objects.
inline bool WaitForWorkerIdle(int timeoutMillis = 2000) {
    auto* engine = ::agenui::getAGenUIEngine();
    if (!engine) {
        return true;  // No engine => trivially idle.
    }

    // Create a temporary SurfaceManager for synchronization
    auto* sm = engine->createSurfaceManager();
    if (!sm) {
        // If createSurfaceManager fails, fall back to sleep-based wait
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMillis));
        return true;
    }

    IdleSignalListener listener;
    sm->addSurfaceEventListener(&listener);

    // Send an empty streaming session to the worker thread.
    // The engine will process this after all prior queued tasks.
    // We send a minimal createSurface protocol to trigger a callback.
    sm->beginTextStream();
    sm->receiveTextChunk(
        R"({"version":"v0.9","createSurface":{"surfaceId":"_idle_sync_","catalogId":"sync","theme":{},"sendDataModel":false,"animated":true}})");
    sm->endTextStream();

    // Wait for the callback to fire — this proves the worker thread
    // has processed all prior tasks including our sync message.
    bool success = listener.waitForSignal(timeoutMillis);

    // Clean up: remove listener and destroy the sync SurfaceManager
    sm->removeSurfaceEventListener(&listener);
    engine->destroySurfaceManager(sm);

    return success;
}

// Spin-wait helper: poll the predicate until it returns true or timeout.
// Useful when we need to observe an asynchronous side effect that may
// require multiple worker-thread hops.
template <typename Pred>
inline bool WaitFor(Pred predicate, int timeoutMillis = 2000,
                    int pollIntervalMillis = 5) {
    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + std::chrono::milliseconds(timeoutMillis);
    while (clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(pollIntervalMillis));
    }
    return predicate();
}

}  // namespace testing
}  // namespace agenui