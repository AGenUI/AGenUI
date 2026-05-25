// Recording IAGenUIMessageListener for tests.
//
// Captures every callback into thread-safe vectors. Tests verify behavior
// by inspecting these vectors after `WaitForWorkerIdle`.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "agenui_dispatcher_types.h"
#include "agenui_message_listener.h"

namespace agenui {
namespace testing {

class MockMessageListener : public ::agenui::IAGenUIMessageListener {
public:
    void onCreateSurface(const ::agenui::CreateSurfaceMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        createSurfaceCalls.push_back(msg);
        ++totalCalls_;
        cv_.notify_all();
    }

    void onDeleteSurface(const ::agenui::DeleteSurfaceMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        deleteSurfaceCalls.push_back(msg);
        ++totalCalls_;
        cv_.notify_all();
    }

    void onComponentsUpdate(
        const std::string& surfaceId,
        const std::vector<::agenui::ComponentsUpdateMessage>& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        componentsUpdateCalls.push_back({surfaceId, msg});
        ++totalCalls_;
        cv_.notify_all();
    }

    void onComponentsAdd(
        const std::string& surfaceId,
        const std::vector<::agenui::ComponentsAddMessage>& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        componentsAddCalls.push_back({surfaceId, msg});
        ++totalCalls_;
        cv_.notify_all();
    }

    void onComponentsRemove(
        const std::string& surfaceId,
        const std::vector<::agenui::ComponentsRemoveMessage>& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        componentsRemoveCalls.push_back({surfaceId, msg});
        ++totalCalls_;
        cv_.notify_all();
    }

    void onActionEventRouted(const std::string& content) override {
        std::lock_guard<std::mutex> lock(mutex_);
        actionEventRoutedCalls.push_back(content);
        ++totalCalls_;
        cv_.notify_all();
    }

    void onError(const ::agenui::ErrorMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        errorCalls.push_back(msg);
        ++totalCalls_;
        cv_.notify_all();
    }

    template <typename Predicate>
    bool waitFor(Predicate pred, int timeoutMillis = 2000) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMillis),
                            pred);
    }

    int totalCalls() const { return totalCalls_.load(); }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        createSurfaceCalls.clear();
        deleteSurfaceCalls.clear();
        componentsUpdateCalls.clear();
        componentsAddCalls.clear();
        componentsRemoveCalls.clear();
        actionEventRoutedCalls.clear();
        errorCalls.clear();
        totalCalls_.store(0);
    }

    // Public so tests can read with the lock held via withLock().
    template <typename F>
    auto withLock(F&& fn) -> decltype(fn(*this)) {
        std::lock_guard<std::mutex> lock(mutex_);
        return fn(*this);
    }

    std::vector<::agenui::CreateSurfaceMessage> createSurfaceCalls;
    std::vector<::agenui::DeleteSurfaceMessage> deleteSurfaceCalls;

    struct ComponentsUpdateRecord {
        std::string surfaceId;
        std::vector<::agenui::ComponentsUpdateMessage> messages;
    };
    struct ComponentsAddRecord {
        std::string surfaceId;
        std::vector<::agenui::ComponentsAddMessage> messages;
    };
    struct ComponentsRemoveRecord {
        std::string surfaceId;
        std::vector<::agenui::ComponentsRemoveMessage> messages;
    };

    std::vector<ComponentsUpdateRecord> componentsUpdateCalls;
    std::vector<ComponentsAddRecord> componentsAddCalls;
    std::vector<ComponentsRemoveRecord> componentsRemoveCalls;
    std::vector<std::string> actionEventRoutedCalls;
    std::vector<::agenui::ErrorMessage> errorCalls;

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::atomic<int> totalCalls_{0};
};

}  // namespace testing
}  // namespace agenui
