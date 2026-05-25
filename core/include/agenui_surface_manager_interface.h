#pragma once

#include <string>
#include "agenui_render_info_types.h"

namespace agenui {

class IAGenUIMessageListener;
struct ActionMessage;
struct SyncUIToDataMessage;

/**
 * @brief SurfaceManager external interface
 *
 * Multi-instance context manager. Each ISurfaceManager corresponds to an independent UI rendering context,
 * with independent card management, data binding, protocol parsing, and event callback chains.
 * Lifecycle is managed by IAGenUIEngine:
 * - Created via IAGenUIEngine::createSurfaceManager()
 * - Destroyed via IAGenUIEngine::destroySurfaceManager()
 *
 * Thread convention:
 * - All external interfaces are called on the business thread
 * - Internal logic is processed on a sub-thread (shared by all SurfaceManager instances), event callbacks to listener are on the sub-thread
 * Data isolation note:
 *  - Each SurfaceManager has independent streaming data buffers and callbacks; different data streams can be bound to different SurfaceManager instances
 */
class ISurfaceManager {
public:
    virtual ~ISurfaceManager() = default;
    /**
     * @brief Gets the instance identifier
     * @return The unique ID assigned at creation
     */
    virtual int getInstanceId() const = 0;
    /**
     * @brief Adds a Surface event listener (internally locked)
     * @param listener Listener pointer (lifecycle managed by the caller)
     */
    virtual void addSurfaceEventListener(IAGenUIMessageListener* listener) = 0;

    /**
     * @brief Removes a Surface event listener (internally locked)
     * @param listener Listener pointer
     * @remark Recommended to call actively before destruction. Engine internal event sending functions are also locked
     */
    virtual void removeSurfaceEventListener(IAGenUIMessageListener* listener) = 0;

    /**
     * @brief Triggers a UI action
     * @param msg Action message
     * @remark Replaces the original EventDispatcher::dispatchAction
     */
    virtual void submitUIAction(const ActionMessage& msg) = 0;

    /**
     * @brief Syncs UI operation data to the data record
     * @param msg SyncUIToData message
     * @remark Replaces the original EventDispatcher::dispatchSyncUIToData
     */
    virtual void submitUIDataModel(const SyncUIToDataMessage& msg) = 0;

    /**
     * @brief Begins a round of A2UI protocol streaming data reception
     *
     * Called by the business side before starting a new round of streaming data transmission.
     * The SDK will clear the internal buffer and reset the parsing state.
     *
     * Call timing:
     * - Before a new round of streaming data begins
     * - Before the first call to receiveTextChunk
     *
     * Interface contract:
     * - Recommended call sequence: beginTextStream → receiveTextChunk × N → endTextStream
     * - If receiveTextChunk is called directly without calling this method, the SDK still works (compatibility mode),
     *   but residual state from the previous round may not be cleaned up
     *
     * Thread convention: Call on the same thread as receiveTextChunk
     */
    virtual void beginTextStream() = 0;

    /**
     * @brief Ends a round of A2UI protocol streaming data reception session
     *
     * Called by the business side after all streaming data in a round has been sent.
     * The SDK will check for any unparseable residual data, clear the buffer, and reset the parsing state.
     *
     * Call timing:
     * - SSE stream normally closes (stream close / EOF)
     * - HTTP response ends
     * - User actively aborts the current conversation
     * - Cleanup after abnormal network disconnection
     *
     * Interface contract:
     * - After calling, the SDK returns to its initial state and can safely start the next round of beginTextStream
     */
    virtual void endTextStream() = 0;
    /**
     * @brief Receives A2UI protocol data
     * @param data A2UI protocol data string
     * @remark Replaces the original transmitRawProtocolStreaming
     */
    virtual void receiveTextChunk(const std::string& data) = 0;

    /**
     * @brief Receives a component render-finish callback from platform code.
     */
    virtual void onRenderFinish(const ComponentRenderInfo& info) = 0;

    /**
     * @brief Receives a surface size change callback from platform code.
     */
    virtual void onSurfaceSizeChanged(const SurfaceLayoutInfo& info) = 0;
    
    /**
     * @brief Re-evaluate every component's attributes and styles, then emit
     *        field-level diffs to the native renderer for any value that
     *        actually changed.
     *
     * Call this when external state owned by the host application has changed
     * in ways the SDK cannot observe (theme, locale, orientation, etc.) and
     * registered FunctionCalls that read from that state need to be re-run.
     *
     * Action handlers (Action.event / Action.functionCall) are not in scope:
     * they are evaluated lazily on event firing, not pre-computed.
     */
    virtual void invalidateFunctionCallValues() = 0;
};

} // namespace agenui
