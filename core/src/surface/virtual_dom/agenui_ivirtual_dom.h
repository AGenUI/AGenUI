#pragma once

#include "agenui_component_snapshot.h"
#include "surface/agenui_layout_direction.h"
#include <string>

namespace agenui {

class BatchGuard;

/**
 * @brief Virtual DOM interface
 * @remark Defines the basic operations of the virtual DOM
 *
 * Callers that issue many updateNode() calls in a single tick should wrap
 * them in a BatchScope via batchGuard(). The implementation suppresses
 * checkAndNotifyLayoutChanges() during the window and emits a single
 * consolidated notification when the outermost batch closes.
 */
class IVirtualDOM {
public:
    virtual ~IVirtualDOM() = default;

    /**
     * @brief Update a node
     * @param snapshot Component snapshot
     * @remark Updates or creates a virtual DOM node
     */
    virtual void updateNode(const ComponentSnapshot& snapshot) = 0;

    /**
     * @brief Clear the virtual DOM tree
     */
    virtual void clear() = 0;

    /**
     * @brief Access the batch guard for this virtual DOM.
     * @return Non-owning pointer to the internal BatchGuard; never null
     *         for a live instance.
     */
    virtual BatchGuard* batchGuard() = 0;

    /**
     * @brief Set the surface-level writing direction.
     *
     * The surface-level value is the single direction source for the whole
     * tree: node-level DSL "direction" styles are NOT supported (they pass
     * through as ordinary styles and never reach the layout engine). If the
     * tree already has content, a re-layout pass runs immediately and
     * changes are notified downstream.
     */
    virtual void setLayoutDirection(LayoutDirection direction) = 0;

    /**
     * @brief Creation-time direction initialization.
     *
     * Configures the direction for a freshly built (still empty) tree
     * WITHOUT the immediate re-layout and surface-size bootstrap fetch that
     * setLayoutDirection performs: at creation time the platform surface
     * does not exist yet, so a layout pass would be wasted work and the
     * one-shot size pull would cache a not-yet-measurable {0,0}.
     *
     * The default implementation falls back to setLayoutDirection for
     * implementations that do not distinguish the two entry points.
     */
    virtual void initLayoutDirection(LayoutDirection direction) {
        setLayoutDirection(direction);
    }
};

}  // namespace agenui