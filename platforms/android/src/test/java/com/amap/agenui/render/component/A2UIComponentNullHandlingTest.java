package com.amap.agenui.render.component;

import android.content.Context;
import android.view.View;

import org.junit.Test;

import java.util.HashMap;
import java.util.Map;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

/**
 * TDD tests for AGenUI null (delete-signal) handling alignment on Android.
 *
 * <p>Per PRD {@code agenui-null-handling-alignment.md}: the Android base class must
 * NOT store null values into the properties map — null is the delete signal, so the
 * key must be removed. Null is still passed through to {@code onUpdateProperties}
 * so leaf components can reset to the type-empty value. {@code action=null} must
 * remove the click listener (delete signal), while {@code action=<value>} sets it.
 *
 * <p>Seams under test (agreed upfront):
 * 1. {@link A2UIComponent#updateProperties} — properties map state after a null value
 *    (key removed, not stored as null).
 * 2. {@code action=null} — click listener removed (setupClickListener else branch).
 * 3. null transparency — {@code onUpdateProperties} receives the null value unchanged.
 *
 * <p>Pure-JVM tests: {@code unitTests.returnDefaultValues = true} lets Android View
 * stubs no-op, so we inject a Mockito mock View and verify interactions.
 */
public class A2UIComponentNullHandlingTest {

    /**
     * Minimal concrete subclass that records received diffs and allows view injection,
     * so the base-class template method can be exercised without triggering createView's
     * Yoga/layout side effects.
     */
    static final class TestComponent extends A2UIComponent {
        Map<String, Object> receivedProps;

        TestComponent(String id) {
            super(id, "Test");
        }

        @Override
        protected View onCreateView(Context context) {
            return mock(View.class);
        }

        @Override
        protected void onUpdateProperties(Map<String, Object> changedProps) {
            this.receivedProps = new HashMap<>(changedProps);
        }

        void injectView(View v) {
            this.view = v;
        }

        Map<String, Object> props() {
            return this.properties;
        }
    }

    private static Map<String, Object> map(Object... kv) {
        Map<String, Object> m = new HashMap<>();
        for (int i = 0; i + 1 < kv.length; i += 2) {
            m.put((String) kv[i], kv[i + 1]);
        }
        return m;
    }

    // ========================================================================
    // Slice 1 — null (delete signal) removes the key from properties
    // ========================================================================

    @Test
    public void updateProperties_nullValue_removesKeyFromProperties() {
        TestComponent c = new TestComponent("t1");
        c.updateProperties(map("text", "hello"));
        assertEquals("hello", c.props().get("text"));

        c.updateProperties(map("text", null));
        assertFalse("null (delete signal) must remove the key from properties, not store null",
                c.props().containsKey("text"));
    }

    @Test
    public void updateProperties_nonNullValue_keepsKeyInProperties() {
        TestComponent c = new TestComponent("t1");
        c.updateProperties(map("text", "hello"));
        c.updateProperties(map("text", "world"));
        assertEquals("world", c.props().get("text"));
    }

    // ========================================================================
    // Slice 2 — action=null removes the click listener; action=<value> sets it
    // ========================================================================

    @Test
    public void updateProperties_actionValue_setsClickListener() {
        TestComponent c = new TestComponent("t1");
        View mockView = mock(View.class);
        c.injectView(mockView);

        c.updateProperties(map("action", map("type", "click")));

        verify(mockView).setOnClickListener(any());
        verify(mockView).setClickable(true);
        verify(mockView).setFocusable(true);
        verify(mockView, never()).setOnClickListener(null);
    }

    @Test
    public void updateProperties_actionNull_removesClickListener() {
        TestComponent c = new TestComponent("t1");
        View mockView = mock(View.class);
        c.injectView(mockView);

        c.updateProperties(map("action", map("type", "click")));
        c.updateProperties(map("action", null));

        verify(mockView).setOnClickListener(null);
        verify(mockView).setClickable(false);
        verify(mockView).setFocusable(false);
    }

    // ========================================================================
    // Slice 3 — null transparency: onUpdateProperties receives null unchanged
    // ========================================================================

    @Test
    public void updateProperties_nullValue_passesNullToOnUpdateProperties() {
        TestComponent c = new TestComponent("t1");
        c.injectView(mock(View.class));

        c.updateProperties(map("text", null));

        assertNotNull(c.receivedProps);
        assertTrue(c.receivedProps.containsKey("text"));
        assertNull(c.receivedProps.get("text"));
    }
}
