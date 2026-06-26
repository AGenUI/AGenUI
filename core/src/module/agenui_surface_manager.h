#pragma once

#include <atomic>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "agenui_surface_manager_interface.h"
#include "agenui_surface_size_provider.h"

namespace agenui {

class StreamingContentParser;
class SurfaceCoordinator;
class EventDispatcher;
class IAGenUIMessageListener;
class ISurfaceSizeProvider;
class IThread;

/**
 * @brief SurfaceManager implementation (formerly AGenUIRuntimeContext)
 *
 * Per-instance context manager that owns all multi-instance modules for one UI rendering context:
 * - EventDispatcher (member object)
 * - StreamingContentParser
 * - SurfaceCoordinator
 *
 * Lifecycle managed by AGenUIEngine.
 *
 * Threading model:
 * - All SurfaceManager instances share one worker thread (AGENUI_SHARED_THREAD_ID)
 * - The shared worker thread is created/destroyed by AGenUIEngine on start/stop
 * - Public APIs that receive data or change state are called on the main thread (e.g. receiveTextChunk)
 * - Internal logic is dispatched to the shared worker thread via post()
 */
class SurfaceManager : public ISurfaceManager,
                       public std::enable_shared_from_this<SurfaceManager> {
public:
    /**
     * @brief Constructor.
     * @param instanceId Instance identifier
     */
    explicit SurfaceManager(int instanceId);
    ~SurfaceManager();

    bool enterRunning();

    bool exitRunning();

    /**
     * @brief Initializes all internal modules.
     */
    bool init();

    /**
     * @brief Stops all internal modules and releases resources.
     */
    void uninit();

    int getInstanceId() const override { return _instanceId; }
    void addSurfaceEventListener(IAGenUIMessageListener* listener) override;
    void removeSurfaceEventListener(IAGenUIMessageListener* listener) override;
    void submitUIAction(const ActionMessage& msg) override;
    void submitUIDataModel(const SyncUIToDataMessage& msg) override;

    /**
     * @brief Begins a streaming session for A2UI protocol data.
     *
     * Call before starting a new streaming session; the SDK clears buffers and resets parse state.
     *
     * When to call:
     * - Before a new stream begins
     * - Before the first receiveTextChunk call
     *
     * Contract:
     * - Recommended sequence: beginTextStream → receiveTextChunk × N → endTextStream
     * - If skipped, receiveTextChunk still works (compatibility mode),
     *   but leftover state from the previous session may not be cleaned up
     *
     * Thread: must be called on the same thread as receiveTextChunk
     */
    void beginTextStream() override;

    /**
     * @brief Ends a streaming session.
     *
     * Call after all stream data has been delivered; the SDK checks for leftover unprocessed data
     * and clears buffers and resets parse state.
     *
     * When to call:
     * - SSE stream closed normally (stream close / EOF)
     * - HTTP response ended
     * - User aborted the current session
     * - Cleanup after network disconnection
     *
     * Contract:
     * - After this call the SDK returns to initial state and beginTextStream may be called again
     */
    void endTextStream() override;
    void receiveTextChunk(const std::string& data) override;

    /**
     * @brief Component render-complete callback (triggered on main thread, posted to worker thread).
     */
    void onRenderFinish(const ComponentRenderInfo& info) override;

    /**
     * @brief Surface size change callback (triggered on main thread, posted to worker thread).
     */
    void onSurfaceSizeChanged(const SurfaceLayoutInfo& info) override;

    /**
     * @brief Re-evaluates every component's attributes and styles across all
     *        surfaces; executes on the worker thread.
     */
    void invalidateFunctionCallValues() override;

    /**
     * @brief Inject the host-supplied source of truth for per-surface sizes.
     *
     * Non-owning. See ISurfaceManager for the threading and lifetime contract.
     */
    void setSurfaceSizeProvider(ISurfaceSizeProvider* provider) override;
    /**
     * @brief Thread-safe surface-size lookup.
     *
     * Holds the same mutex as setSurfaceSizeProvider() across the provider
     * dispatch, so callers cannot race with a host detach/destroy. Use this
     * instead of pulling out the raw provider pointer.
     *
     * @return std::nullopt when no provider is currently injected.
     */
    std::optional<SurfaceSize> getSurfaceSize(const std::string& surfaceId) const;

    EventDispatcher* getEventDispatcher();
    StreamingContentParser* getStreamingContentParser();
    SurfaceCoordinator* getSurfaceCoordinator();

    IThread* getMessageThread();

    bool isRunning() const { return _isRunning.load(); }

private:
    void createStreamingContentParser();
    void createSurfaceCoordinator();

    void destroySurfaceCoordinator();
    void destroyStreamingContentParser();

    int _instanceId;

    // Multi-instance modules (owned)
    EventDispatcher* _dispatcher = nullptr;
    std::mutex _cachedListenersMutex;
    std::vector<IAGenUIMessageListener*> _cachedListeners;
    StreamingContentParser* _streamingContentParser = nullptr;
    SurfaceCoordinator* _surfaceCoordinator = nullptr;

    // Host-supplied surface size source of truth. Non-owning; the host is
    // responsible for keeping it alive at least until this SurfaceManager
    // is destroyed or a subsequent setSurfaceSizeProvider(nullptr) call
    // detaches it. Read on the worker thread; updates are serialized by
    // _surfaceSizeProviderMutex against any in-flight provider queries.
    ISurfaceSizeProvider* _surfaceSizeProvider = nullptr;
    mutable std::mutex _surfaceSizeProviderMutex;

    // Running state
    std::atomic_bool _isRunning{false};
};

} // namespace agenui
