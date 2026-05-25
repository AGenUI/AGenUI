#pragma once

#include "agenui_ithread.h"
#include <mutex>
#include <unordered_map>

namespace agenui {

/**
 * @brief Thread manager (singleton)
 *
 * Manages worker threads keyed by threadId.
 * Currently all SurfaceManager instances share a single MessageThread identified by
 * AGENUI_SHARED_THREAD_ID. The shared thread is created by AGenUIEngine on start
 * and destroyed on stop.
 *
 * The map-based storage is designed to be extensible, allowing multiple independent
 * threads to be registered in the future (e.g. one dedicated thread per engine instance).
 *
 * Thread safety:
 * - Thread map is protected by a mutex
 * - Thread creation and destruction are managed centrally
 */
class ThreadManager {
public:
    /**
     * @brief Returns the singleton instance.
     */
    static ThreadManager& getInstance();

    /**
     * @brief Creates a worker thread for the given threadId.
     * @param threadId
     * @return true if successful
     */
    bool createThread(int threadId);

    /**
     * @brief Destroys the worker thread for the given threadId.
     * @param threadId
     */
    void destroyThread(int threadId);

    /**
     * @brief Returns the message thread for the given threadId.
     * @param threadId
     * @return Thread pointer, or nullptr if not started
     */
    IThread* getMessageThread(int threadId);

private:
    ThreadManager() = default;
    ~ThreadManager();
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    std::unordered_map<int, IThread*> _threads;
    std::mutex _mutex;
};

} // namespace agenui
