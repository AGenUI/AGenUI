package com.amap.agenui.render.utils;

import static org.junit.Assert.*;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.lang.reflect.Field;

/**
 * Unit tests for {@link ImageTransitionManager}.
 * Tests state management: get/set transition, get/set duration, null safety.
 */
public class ImageTransitionManagerTest {

    private ImageTransition originalTransition;
    private long originalDuration;

    @Before
    public void setUp() throws Exception {
        // Save original state
        originalTransition = ImageTransitionManager.getDefaultTransition();
        originalDuration = ImageTransitionManager.getDefaultDuration();
    }

    @After
    public void tearDown() throws Exception {
        // Restore original state via reflection to avoid polluting other tests
        Field transitionField = ImageTransitionManager.class.getDeclaredField("defaultTransition");
        transitionField.setAccessible(true);
        transitionField.set(null, originalTransition);

        Field durationField = ImageTransitionManager.class.getDeclaredField("defaultDuration");
        durationField.setAccessible(true);
        durationField.set(null, originalDuration);
    }

    // ─── Default values ─────────────────────────────────────────────────────

    @Test
    public void defaultTransition_isMagicRevealTransition() {
        ImageTransition t = ImageTransitionManager.getDefaultTransition();
        assertNotNull(t);
        assertTrue(t instanceof MagicRevealTransition);
    }

    @Test
    public void defaultDuration_is1000ms() {
        assertEquals(1000L, ImageTransitionManager.getDefaultDuration());
    }

    // ─── setDefaultTransition ────────────────────────────────────────────────

    @Test
    public void setDefaultTransition_customImpl_isReturned() {
        ImageTransition custom = new TestTransition();
        ImageTransitionManager.setDefaultTransition(custom);
        assertSame(custom, ImageTransitionManager.getDefaultTransition());
    }

    @Test
    public void setDefaultTransition_null_doesNotChangeExisting() {
        ImageTransition before = ImageTransitionManager.getDefaultTransition();
        ImageTransitionManager.setDefaultTransition(null);
        assertSame(before, ImageTransitionManager.getDefaultTransition());
    }

    @Test
    public void setDefaultTransition_multipleChanges_lastWins() {
        ImageTransition first = new TestTransition();
        ImageTransition second = new TestTransition();
        ImageTransitionManager.setDefaultTransition(first);
        ImageTransitionManager.setDefaultTransition(second);
        assertSame(second, ImageTransitionManager.getDefaultTransition());
    }

    // ─── setDefaultDuration ──────────────────────────────────────────────────

    @Test
    public void setDefaultDuration_customValue_isReturned() {
        ImageTransitionManager.setDefaultDuration(2500L);
        assertEquals(2500L, ImageTransitionManager.getDefaultDuration());
    }

    @Test
    public void setDefaultDuration_zero_isAccepted() {
        ImageTransitionManager.setDefaultDuration(0L);
        assertEquals(0L, ImageTransitionManager.getDefaultDuration());
    }

    @Test
    public void setDefaultDuration_negative_isAccepted() {
        // The API does not prevent negative values
        ImageTransitionManager.setDefaultDuration(-100L);
        assertEquals(-100L, ImageTransitionManager.getDefaultDuration());
    }

    @Test
    public void setDefaultDuration_maxLong_isAccepted() {
        ImageTransitionManager.setDefaultDuration(Long.MAX_VALUE);
        assertEquals(Long.MAX_VALUE, ImageTransitionManager.getDefaultDuration());
    }

    // ─── Test helper ─────────────────────────────────────────────────────────

    /**
     * Minimal ImageTransition implementation for testing.
     */
    private static class TestTransition implements ImageTransition {
        @Override
        public void animate(android.widget.ImageView imageView, long duration, Runnable completion) {
            // no-op for testing
        }

        @Override
        public long getDefaultDuration() {
            return 800;
        }
    }
}
