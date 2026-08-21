package com.amap.agenui.render.component;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * Per-key dirty tracking for incremental property updates.
 *
 * <p>Aligned with HarmonyOS {@code a2ui::ComponentState}. Stores a shadow
 * copy of property values and tracks which keys have actually changed since
 * the last {@link #clearDirty()} call, so the update path can skip
 * re-applying unchanged attributes.
 */
public class ComponentState {

    private final String componentId;
    private final Map<String, Object> lastValues = new HashMap<>();
    private final Set<String> dirtyKeys = new HashSet<>();

    public ComponentState(String componentId) {
        this.componentId = componentId;
    }

    /**
     * Compares each key in {@code diff} against the last-seen value.
     * Only keys whose values actually differ are marked dirty.
     */
    public void updateProperties(Map<String, Object> diff) {
        if (diff == null) {
            return;
        }
        for (Map.Entry<String, Object> entry : diff.entrySet()) {
            String key = entry.getKey();
            Object newValue = entry.getValue();
            // A key seen for the first time is always a change — even when the
            // value is null, because null is the delete signal (PRD
            // agenui-null-handling-alignment.md) and must reach onUpdateProperties.
            // Only skip when the key already existed and the value is genuinely
            // equal, so "from-absent to null" is not swallowed as a no-op.
            boolean hadKey = lastValues.containsKey(key);
            if (hadKey && Objects.equals(lastValues.get(key), newValue)) {
                continue;
            }
            lastValues.put(key, newValue);
            dirtyKeys.add(key);
        }
    }

    public boolean isDirty() {
        return !dirtyKeys.isEmpty();
    }

    public void clearDirty() {
        dirtyKeys.clear();
    }
}
