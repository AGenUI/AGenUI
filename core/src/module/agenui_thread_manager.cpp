#include "agenui_thread_manager.h"
#include "agenui_message_thread.h"
#include "agenui_logger_internal.h"
#include "agenui_type_define.h"

namespace agenui {

ThreadManager& ThreadManager::getInstance() {
    static ThreadManager instance;
    return instance;
}

ThreadManager::~ThreadManager() {
    for (auto& pair : _threads) {
        pair.second->stop();
    }
    _threads.clear();
}

bool ThreadManager::createThread(int threadId) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_threads.find(threadId) != _threads.end()) {
        AGENUI_LOG("%d already exists", threadId);
        return true;
    }

    std::string name = "AGenUI-" + std::to_string(threadId);
    // Order: create → start → insert into map
    auto newThread = std::make_shared<MessageThread>(name);
    newThread->start();
    _threads[threadId] = newThread;
    AGENUI_LOG("created thread '%s'", name.c_str());
    return true;
}

void ThreadManager::destroyThread(int threadId) {
    AGENUI_LOG("begin destroying thread for %d", threadId);
    std::shared_ptr<IThread> thread;
    // Order: erase → stop → release. stop() (join) must complete before
    // releasing our reference: ~MessageThread must never see a joinable thread.
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _threads.find(threadId);
        if (it == _threads.end()) {
            return;
        }
        thread = it->second;
        _threads.erase(it);
    }
    thread->stop();
    AGENUI_LOG("destroyed %d", threadId);
}

std::shared_ptr<IThread> ThreadManager::getMessageThread(int threadId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _threads.find(threadId);
    if (it == _threads.end()) {
        return nullptr;
    }
    return it->second;
}

} // namespace agenui
