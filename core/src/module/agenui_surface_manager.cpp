#include <algorithm>
#include "agenui_surface_manager.h"
#include "agenui_logger_internal.h"
#include "agenui_surface_size_provider.h"
#include "agenui_type_define.h"
#include "agenui_thread_manager.h"
#include "stream/agenui_streaming_content_parser.h"
#include "surface/agenui_surface_coordinator.h"
#include "module/agenui_event_dispatcher.h"

namespace agenui {

SurfaceManager::SurfaceManager(int instanceId)
    : _instanceId(instanceId) {
}

SurfaceManager::~SurfaceManager() {
    uninit();
}

bool SurfaceManager::enterRunning() {
    AGENUI_LOG("enter running, %d", _instanceId);
    if (_isRunning.load()) {
        return false;
    }
    _isRunning.store(true);
    return true;
}

bool SurfaceManager::exitRunning() {
    AGENUI_LOG("exit running, %d", _instanceId);
    if (!_isRunning.load()) {
        return false;
    }
    _isRunning.store(false);
    return true;
}

bool SurfaceManager::init() {
    AGENUI_LOG("init, %d", _instanceId);
    // Create EventDispatcher and flush cached listeners under the same lock
    // to prevent race with addSurfaceEventListener/removeSurfaceEventListener
    {
        std::lock_guard<std::mutex> mutexWrap(_cachedListenersMutex);
        _dispatcher = new EventDispatcher();
        for (const auto &listener : _cachedListeners) {
            AGENUI_LOG("add cached listener %p", listener);
            _dispatcher->addEventListener(listener);
        }
        _cachedListeners.clear();
    }

    // Create modules in dependency order
    createSurfaceCoordinator();
    createStreamingContentParser();
    return true;
}

void SurfaceManager::uninit() {
    AGENUI_LOG("uninit, %d", _instanceId);
    // Remove all listeners first to prevent callbacks during teardown
    {
        std::lock_guard<std::mutex> mutexWrap(_cachedListenersMutex);
        _cachedListeners.clear();
    }
    if (_dispatcher) {
        _dispatcher->removeAllEventListeners();
    }

    // Destroy in strict reverse order
    destroyStreamingContentParser();
    destroySurfaceCoordinator();

    // Destroy EventDispatcher
    SAFELY_DELETE(_dispatcher);
}

void SurfaceManager::addSurfaceEventListener(IAGenUIMessageListener* listener) {
    if (!_isRunning.load()) {
        return;
    }
    // Lock ensures _dispatcher visibility is consistent with init() thread
    std::lock_guard<std::mutex> mutexWrap(_cachedListenersMutex);
    if (_dispatcher) {
        AGENUI_LOG("add lisenter success %d, listener:%p, %p", _instanceId, listener, _dispatcher);
        _dispatcher->addEventListener(listener);
    } else {
        AGENUI_LOG("add lisenter to cache %d, listener:%p", _instanceId, listener);
        _cachedListeners.push_back(listener);
    }
}

void SurfaceManager::removeSurfaceEventListener(IAGenUIMessageListener* listener) {
    if (!_isRunning.load()) {
        return;
    }
    // Lock ensures _dispatcher visibility is consistent with init() thread
    std::lock_guard<std::mutex> mutexWrap(_cachedListenersMutex);
    if (_dispatcher) {
        AGENUI_LOG("remove listener success %d, listener:%p, %p", _instanceId, listener, _dispatcher);
        _dispatcher->removeEventListener(listener);
    } else {
        AGENUI_LOG("remove listener from cache %d, listener:%p", _instanceId, listener);
        _cachedListeners.erase(
            std::remove(_cachedListeners.begin(), _cachedListeners.end(), listener),
            _cachedListeners.end());
    }
}

void SurfaceManager::submitUIAction(const ActionMessage& msg) {
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("MessageThread is null, submitUIAction ignored");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self, msg]() {
        if (self->_surfaceCoordinator) {
            self->_surfaceCoordinator->handleAction(msg);
        }
    });
}

void SurfaceManager::submitUIDataModel(const SyncUIToDataMessage& msg) {
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("MessageThread is null, submitUIDataModel ignored");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self, msg]() {
        if (self->_surfaceCoordinator) {
            self->_surfaceCoordinator->handleSyncUIToData(msg);
        }
    });
}

void SurfaceManager::beginTextStream() {
    AGENUI_LOG("begin text stream");
    AGENUI_PERFORMANCE_LOG("surface_begin_text_stream", "instanceId=%d", _instanceId);
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("MessageThread is null, beginTextStream ignored");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self]() {
        if (self->_streamingContentParser) {
            self->_streamingContentParser->processDataBeginning();
        }
    });
}

void SurfaceManager::endTextStream() {
    AGENUI_LOG("end text stream");
    AGENUI_PERFORMANCE_LOG("surface_end_text_stream", "instanceId=%d", _instanceId);
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("MessageThread is null, endTextStream ignored");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self]() {
        if (self->_streamingContentParser) {
            self->_streamingContentParser->processDataEnding();
        }
    });
}

void SurfaceManager::receiveTextChunk(const std::string& data) {
    AGENUI_PERFORMANCE_LOG("surface_receive_text_chunk", "instanceId=%d, bytes=%zu", _instanceId, data.size());
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("MessageThread is null, receiveTextChunk ignored");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self, data]() {
        if (self->_streamingContentParser) {
            self->_streamingContentParser->processDataAssembling(data);
        }
    });
}

void SurfaceManager::onRenderFinish(const ComponentRenderInfo& info) {
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("onRenderFinish: messageThread is null, drop");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self, info]() {
        if (self->_surfaceCoordinator) {
            self->_surfaceCoordinator->handleRenderFinish(info);
        }
    });
}

void SurfaceManager::onSurfaceSizeChanged(const SurfaceLayoutInfo& info) {
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        AGENUI_LOG("onSurfaceSizeChanged: messageThread is null, drop");
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self, info]() {
        if (self->_surfaceCoordinator) {
            self->_surfaceCoordinator->handleSurfaceSizeChanged(info);
        }
    });
}

void SurfaceManager::invalidateFunctionCallValues() {
    if (!_isRunning.load()) {
        return;
    }
    IThread* messageThread = getMessageThread();
    if (!messageThread) {
        return;
    }
    auto self = shared_from_this();
    messageThread->post([self]() {
        if (self->_surfaceCoordinator) {
            self->_surfaceCoordinator->invalidateFunctionCallValues();
        }
    });
}

void SurfaceManager::setSurfaceSizeProvider(ISurfaceSizeProvider* provider) {
    std::lock_guard<std::mutex> lock(_surfaceSizeProviderMutex);
    _surfaceSizeProvider = provider;
}

std::optional<SurfaceSize> SurfaceManager::getSurfaceSize(const std::string& surfaceId) const {
    // Hold the lock across the provider dispatch to prevent a concurrent
    // setSurfaceSizeProvider() from leaving us with a dangling pointer.
    std::lock_guard<std::mutex> lock(_surfaceSizeProviderMutex);
    if (!_surfaceSizeProvider) {
        return std::nullopt;
    }
    return _surfaceSizeProvider->getSurfaceSize(surfaceId);
}

EventDispatcher* SurfaceManager::getEventDispatcher() {
    return _dispatcher;
}

StreamingContentParser* SurfaceManager::getStreamingContentParser() {
    return _streamingContentParser;
}

SurfaceCoordinator* SurfaceManager::getSurfaceCoordinator() {
    return _surfaceCoordinator;
}

IThread* SurfaceManager::getMessageThread() {
    return ThreadManager::getInstance().getMessageThread(AGENUI_SHARED_THREAD_ID);
}

void SurfaceManager::createStreamingContentParser() {
    _streamingContentParser = new StreamingContentParser(_surfaceCoordinator);
    _streamingContentParser->start();
}

void SurfaceManager::createSurfaceCoordinator() {
    _surfaceCoordinator = new SurfaceCoordinator(this);
}
void SurfaceManager::destroySurfaceCoordinator() {
    SAFELY_DELETE(_surfaceCoordinator);
}

void SurfaceManager::destroyStreamingContentParser() {
    if (_streamingContentParser != nullptr) {
        _streamingContentParser->stop();
        SAFELY_DELETE(_streamingContentParser);
    }
}

} // namespace agenui
