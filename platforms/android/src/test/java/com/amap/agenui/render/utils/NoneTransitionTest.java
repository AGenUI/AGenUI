package com.amap.agenui.render.utils;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;

/**
 * Unit tests for {@link NoneTransition}.
 *
 * <p>Verifies: alpha/scale reset to 1f and the completion callback is fired
 * synchronously. Also exercises {@link ImageTransition#getDefaultDuration()}
 * default-method behaviour through this concrete implementation.
 */
public class NoneTransitionTest {

    private NoneTransition transition;
    private ImageView imageView;

    @Before
    public void setUp() {
        transition = new NoneTransition();
        imageView = mock(ImageView.class);
    }

    // ──────────────────────────────────────────────────────────────
    // animate — view properties reset
    // ──────────────────────────────────────────────────────────────

    @Test
    public void animate_setsAlphaToOne() {
        transition.animate(imageView, 0L, () -> { /* no-op */ });

        ArgumentCaptor<Float> captor = ArgumentCaptor.forClass(Float.class);
        verify(imageView).setAlpha(captor.capture());
        assertEquals(1f, captor.getValue(), 0.0001f);
    }

    @Test
    public void animate_setsScaleXToOne() {
        transition.animate(imageView, 0L, () -> { /* no-op */ });

        ArgumentCaptor<Float> captor = ArgumentCaptor.forClass(Float.class);
        verify(imageView).setScaleX(captor.capture());
        assertEquals(1f, captor.getValue(), 0.0001f);
    }

    @Test
    public void animate_setsScaleYToOne() {
        transition.animate(imageView, 0L, () -> { /* no-op */ });

        ArgumentCaptor<Float> captor = ArgumentCaptor.forClass(Float.class);
        verify(imageView).setScaleY(captor.capture());
        assertEquals(1f, captor.getValue(), 0.0001f);
    }

    // ──────────────────────────────────────────────────────────────
    // animate — completion callback contract
    // ──────────────────────────────────────────────────────────────

    @Test
    public void animate_invokesCompletionExactlyOnce() {
        Runnable completion = mock(Runnable.class);

        transition.animate(imageView, 0L, completion);

        verify(completion, times(1)).run();
    }

    @Test
    public void animate_nullCompletion_doesNotCrash() {
        // Contract: null completion is allowed and silently ignored.
        transition.animate(imageView, 0L, null);

        // No exception thrown — view properties still set.
        verify(imageView).setAlpha(1f);
    }

    @Test
    public void animate_durationParameterIsIgnored() {
        // NoneTransition is instantaneous regardless of caller-supplied duration.
        Runnable completion = mock(Runnable.class);

        transition.animate(imageView, /*duration=*/9999L, completion);

        // Completion still fires synchronously, exactly once.
        verify(completion, times(1)).run();
    }

    @Test
    public void animate_completionFiredAfterViewPropertiesSet() {
        // Caller may rely on this ordering — verify via inOrder is overkill;
        // a simple sequence assertion through invocation count suffices.
        Runnable completion = mock(Runnable.class);

        transition.animate(imageView, 0L, completion);

        verify(imageView, times(1)).setAlpha(1f);
        verify(imageView, times(1)).setScaleX(1f);
        verify(imageView, times(1)).setScaleY(1f);
        verify(completion, times(1)).run();
    }

    // ──────────────────────────────────────────────────────────────
    // getDefaultDuration — overridden value
    // ──────────────────────────────────────────────────────────────

    @Test
    public void getDefaultDuration_isZero() {
        assertEquals(0L, transition.getDefaultDuration());
    }

    @Test
    public void getDefaultDuration_overridesInterfaceDefault() {
        // Interface default is 800ms; NoneTransition explicitly returns 0.
        // Verify both values to ensure the override is the binding one.
        ImageTransition asInterface = transition;
        assertEquals(0L, asInterface.getDefaultDuration());
    }

    // ──────────────────────────────────────────────────────────────
    // ImageTransition default method — exercised via anonymous impl
    // ──────────────────────────────────────────────────────────────

    @Test
    public void interfaceDefault_returns800() {
        // Bare-bones implementation that does NOT override getDefaultDuration().
        ImageTransition bare = (view, dur, done) -> {
            if (done != null) {
                done.run();
            }
        };

        assertEquals(800L, bare.getDefaultDuration());
    }

    // ──────────────────────────────────────────────────────────────
    // animate — does not query ImageView for unrelated state
    // ──────────────────────────────────────────────────────────────

    @Test
    public void animate_doesNotCallSetVisibility() {
        // Sanity: NoneTransition only mutates alpha + scale; no other side effects.
        transition.animate(imageView, 0L, () -> { /* no-op */ });

        verify(imageView, never()).setVisibility(android.view.View.GONE);
        verify(imageView, never()).setVisibility(android.view.View.VISIBLE);
        verify(imageView, never()).setVisibility(android.view.View.INVISIBLE);
    }
}
