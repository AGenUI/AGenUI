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
        SAFELY_DELETE(pair.second);
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
    IThread *newThread = new MessageThread(name);
    newThread->start();
    _threads[threadId] = newThread;
    AGENUI_LOG("created thread '%s'", name.c_str());
    return true;
}

void ThreadManager::destroyThread(int threadId) {
    AGENUI_LOG("begin destroying thread for %d", threadId);
    IThread* thread = nullptr;
    // Order: remove → stop → delete (exact reverse of createThread)
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
    SAFELY_DELETE(thread);

    AGENUI_LOG("destroyed %d", threadId);
}

IThread* ThreadManager::getMessageThread(int threadId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _threads.find(threadId);
    if (it == _threads.end()) {
        return nullptr;
    }
    return it->second;
}

} // namespace agenui
