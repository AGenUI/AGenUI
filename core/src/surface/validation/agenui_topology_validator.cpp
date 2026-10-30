#include "surface/validation/agenui_topology_validator.h"

#include <unordered_set>
#include <utility>

namespace agenui {

bool TopologyValidator::wouldIntroduceCycle(const std::string& id,
                                            const std::vector<std::string>& children,
                                            const ChildrenProvider& provider) {
    if (id.empty()) {
        return false;
    }

    // Iterative DFS over the descendants of `children`; a cycle exists iff the
    // walk re-enters `id`. Iterative on purpose: a deep or adversarial graph
    // must not be able to overflow the stack here.
    std::unordered_set<std::string> visited;
    std::vector<std::string> stack(children.begin(), children.end());

    while (!stack.empty()) {
        std::string current = std::move(stack.back());
        stack.pop_back();

        if (current == id) {
            return true;
        }
        if (!visited.insert(current).second) {
            continue;
        }

        std::vector<std::string> next;
        if (!provider(current, next)) {
            continue;  // unknown node: dangling reference, not a cycle
        }
        for (auto& child : next) {
            if (visited.find(child) == visited.end()) {
                stack.push_back(std::move(child));
            }
        }
    }

    return false;
}

}  // namespace agenui
