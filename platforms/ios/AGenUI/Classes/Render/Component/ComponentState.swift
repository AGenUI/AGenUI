import Foundation

/// Per-key dirty tracking for incremental property updates.
///
/// Aligned with Android `ComponentState` and HarmonyOS `a2ui::ComponentState`.
/// Stores last-seen values and tracks which keys actually changed since the
/// last `clearDirty()`, so the update path can skip re-applying unchanged
/// attributes.
class ComponentState {

    private var lastValues: [String: Any] = [:]
    private(set) var dirtyKeys: Set<String> = []

    /// Compares each key in `diff` against the last-seen value.
    /// Only keys whose values actually differ are marked dirty.
    func updateProperties(_ diff: [String: Any]) {
        for (key, newValue) in diff {
            if areEqual(lastValues[key], newValue) {
                continue
            }
            lastValues[key] = newValue
            dirtyKeys.insert(key)
        }
    }

    var isDirty: Bool {
        !dirtyKeys.isEmpty
    }

    func clearDirty() {
        dirtyKeys.removeAll()
    }

    // MARK: - Value comparison

    /// Compares two `Any` values for semantic equality.
    ///
    /// Swift `Any` is not `Equatable`, so we fall back to JSON serialization
    /// for complex types (dictionaries, arrays) and direct comparison for
    /// primitives.
    private func areEqual(_ a: Any?, _ b: Any?) -> Bool {
        switch (a, b) {
        case (nil, nil):
            return true
        case (nil, _), (_, nil):
            return false
        case let (a as String, b as String):
            return a == b
        case let (a as NSNumber, b as NSNumber):
            return a == b
        case let (a as Bool, b as Bool):
            return a == b
        default:
            // Fall back to JSON serialization for dictionaries, arrays, etc.
            guard let dataA = try? JSONSerialization.data(withJSONObject: a!, options: [.sortedKeys]),
                  let dataB = try? JSONSerialization.data(withJSONObject: b!, options: [.sortedKeys]) else {
                return "\(a!)" == "\(b!)"
            }
            return dataA == dataB
        }
    }
}
