#pragma once

#include "agenui_component_snapshot.h"
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
};

}  // namespace agenui