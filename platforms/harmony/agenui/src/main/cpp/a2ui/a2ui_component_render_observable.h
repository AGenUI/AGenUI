#pragma once

#include "agenui_render_info_types.h"
#include <vector>
#include <mutex>

namespace agenui {

/**
 * @brief Listener interface for component render completion events.
 * @remark Harmony-internal type. Other platforms talk to ISurfaceManager directly.
 */
class ComponentRenderListener {
public:
    virtual ~ComponentRenderListener() = default;
    virtual void onRenderFinish(const ComponentRenderInfo& info) = 0;
};

/**
 * @brief Observable interface for fanning out component render completion events.
 * @remark Harmony-internal type. Used by C++ render-layer components (Tabs/Video/Image)
 *         to publish render-finish events without holding an ISurfaceManager pointer.
 */
class IComponentRenderObservable {
public:
    virtual ~IComponentRenderObservable() = default;
    virtual void addComponentRenderListener(ComponentRenderListener* observer) = 0;
    virtual void removeComponentRenderListener(ComponentRenderListener* observer) = 0;
    virtual void notifyRenderFinish(const ComponentRenderInfo& info) = 0;
};

/**
 * @brief Observable implementation for component render completion events.
 * @remark Manages render listeners with multi-observer support.
 *
 * Behavior:
 * - Registers and manages multiple ComponentRenderListener instances
 * - Notifies listeners when rendering finishes
 * - Keeps listener operations thread-safe
 */
class A2UIComponentRenderObservable : public IComponentRenderObservable {
public:
    A2UIComponentRenderObservable();
    ~A2UIComponentRenderObservable() override;

    A2UIComponentRenderObservable(const A2UIComponentRenderObservable&) = delete;
    A2UIComponentRenderObservable& operator=(const A2UIComponentRenderObservable&) = delete;

    /**
     * @brief Add a render listener.
     * @param observer Observer instance pointer
     */
    void addComponentRenderListener(ComponentRenderListener* observer) override;

    /**
     * @brief Remove a render listener.
     * @param observer Observer instance pointer
     */
    void removeComponentRenderListener(ComponentRenderListener* observer) override;

    /**
     * @brief Notify all listeners that rendering has finished.
     * @param info Render details
     */
    void notifyRenderFinish(const ComponentRenderInfo& info) override;

    /**
     * @brief Remove all listeners.
     */
    void clearAllListeners();

private:
    std::vector<ComponentRenderListener*> m_listeners;  // Registered listeners
    mutable std::mutex m_mutex;                         // Guards listener access
};

} // namespace agenui
