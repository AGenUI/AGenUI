#pragma once

#include <functional>
#include <string>
#include <vector>

namespace agenui {

/**
 * @brief Component-graph topology checks, decoupled from the component and
 *        virtual-DOM models.
 *
 * A2UI defines the UI as a tree rooted at "root" (adjacency-list model). A
 * reference cycle is malformed input and is rejected by every official renderer
 * (web_core detectCycles, Swift GraphTopologyValidator). This mirrors that rule
 * so a cyclic component is dropped instead of corrupting the orphan tables.
 *
 * The validator is pure: the caller supplies an adjacency view, so this module
 * has no dependency on ComponentModel / VirtualDOM and stays unit-testable.
 */
class TopologyValidator {
public:
    /**
     * @brief Supplies the child ids of a node.
     * @return false when the node is unknown (a dangling reference, tolerated
     *         during progressive rendering); true with outChildren filled otherwise.
     */
    using ChildrenProvider =
        std::function<bool(const std::string& id, std::vector<std::string>& outChildren)>;

    /**
     * @brief Whether committing `id` with `children` would make `id` reachable
     *        from itself, i.e. introduce a reference cycle.
     *
     * The walk stops as soon as it re-enters `id`, so the node's own previously
     * committed children are irrelevant. Because the rest of the graph is kept
     * acyclic by rejecting every cycle-introducing commit, any new cycle must
     * pass through `id`, which makes this local check sufficient.
     *
     * @param id       Id of the node about to be committed.
     * @param children Child ids the node will reference.
     * @param provider Adjacency view of the already-committed graph.
     * @return true if a cycle (including a direct self-reference) would form.
     */
    static bool wouldIntroduceCycle(const std::string& id,
                                    const std::vector<std::string>& children,
                                    const ChildrenProvider& provider);
};

}  // namespace agenui
