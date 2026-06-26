#pragma once

#include "agenui_render_info_types.h"
#include <vector>
#include <mutex>

namespace agenui {

/**
 * @brief Listener interface for surface layout change events.
 * @remark Harmony-internal type. Other platforms talk to ISurfaceManager directly.
 */
class SurfaceLayoutListener {
public:
    virtual ~SurfaceLayoutListener() = default;
    virtual void onSurfaceSizeChanged(const SurfaceLayoutInfo& info) = 0;
};

/**
 * @brief Observable interface for fanning out surface layout change events.
 * @remark Harmony-internal type.
 */
class ISurfaceLayoutObservable {
public:
    virtual ~ISurfaceLayoutObservable() = default;
    virtual void addSurfaceLayoutListener(SurfaceLayoutListener* listener) = 0;
    virtual void removeSurfaceLayoutListener(SurfaceLayoutListener* listener) = 0;
    virtual void notifySurfaceSizeChanged(const SurfaceLayoutInfo& info) = 0;
};

/**
 * @brief Observable implementation for surface layout changes.
 * @remark Manages surface listeners with multi-observer support.
 *
 * Behavior:
 * - Registers and manages multiple SurfaceLayoutListener instances
 * - Notifies listeners when the surface size changes
 * - Keeps listener operations thread-safe
 */
class A2UISurfaceLayoutObservable : public ISurfaceLayoutObservable {
public:
    A2UISurfaceLayoutObservable();
    ~A2UISurfaceLayoutObservable() override;

    A2UISurfaceLayoutObservable(const A2UISurfaceLayoutObservable&) = delete;
    A2UISurfaceLayoutObservable& operator=(const A2UISurfaceLayoutObservable&) = delete;

    /**
     * @brief Add a surface layout listener.
     * @param listener Listener instance pointer
     */
    void addSurfaceLayoutListener(SurfaceLayoutListener* listener) override;

    /**
     * @brief Remove a surface layout listener.
     * @param listener Listener instance pointer
     */
    void removeSurfaceLayoutListener(SurfaceLayoutListener* listener) override;

    /**
     * @brief Notify all listeners that the surface size changed.
     * @param info Surface layout details
     */
    void notifySurfaceSizeChanged(const SurfaceLayoutInfo& info) override;

    /**
     * @brief Remove all listeners.
     */
    void clearAllListeners();

private:
    std::vector<SurfaceLayoutListener*> m_listeners;  // Registered listeners
    mutable std::mutex m_mutex;                       // Guards listener access
};

} // namespace agenui
