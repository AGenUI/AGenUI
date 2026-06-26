package com.amap.agenui.render.image;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.graphics.drawable.Drawable;

import org.junit.Test;

/**
 * Unit tests for {@link ImageLoadResult} factory methods, constructors,
 * and the {@link ImageLoadResult.Format} enum.
 *
 * <p>Pure data-class assertions — Drawable is mocked via Mockito because
 * its concrete behaviour is irrelevant to ImageLoadResult's contract.
 */
public class ImageLoadResultTest {

    // ──────────────────────────────────────────────────────────────
    // 2-arg constructor
    // ──────────────────────────────────────────────────────────────

    @Test
    public void ctor_twoArg_defaultsFormatToUnknown() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult result = new ImageLoadResult(drawable, false);

        assertEquals(ImageLoadResult.Format.UNKNOWN, result.format);
    }

    @Test
    public void ctor_twoArg_preservesDrawableReference() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult result = new ImageLoadResult(drawable, false);

        assertSame(drawable, result.drawable);
    }

    @Test
    public void ctor_twoArg_preservesIsFromCacheTrue() {
        ImageLoadResult result = new ImageLoadResult(mock(Drawable.class), true);

        assertTrue(result.isFromCache);
    }

    @Test
    public void ctor_twoArg_preservesIsFromCacheFalse() {
        ImageLoadResult result = new ImageLoadResult(mock(Drawable.class), false);

        assertFalse(result.isFromCache);
    }

    // ──────────────────────────────────────────────────────────────
    // 3-arg constructor
    // ──────────────────────────────────────────────────────────────

    @Test
    public void ctor_threeArg_preservesProvidedFormat() {
        ImageLoadResult result = new ImageLoadResult(
                mock(Drawable.class), false, ImageLoadResult.Format.LOTTIE);

        assertEquals(ImageLoadResult.Format.LOTTIE, result.format);
    }

    @Test
    public void ctor_threeArg_preservesAllFields() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult result = new ImageLoadResult(
                drawable, true, ImageLoadResult.Format.SVG);

        assertSame(drawable, result.drawable);
        assertTrue(result.isFromCache);
        assertEquals(ImageLoadResult.Format.SVG, result.format);
    }

    // ──────────────────────────────────────────────────────────────
    // bitmap() factory
    // ──────────────────────────────────────────────────────────────

    @Test
    public void bitmap_factory_setsBitmapFormat() {
        ImageLoadResult result = ImageLoadResult.bitmap(mock(Drawable.class), false);

        assertEquals(ImageLoadResult.Format.BITMAP, result.format);
    }

    @Test
    public void bitmap_factory_preservesCacheFlagTrue() {
        ImageLoadResult result = ImageLoadResult.bitmap(mock(Drawable.class), true);

        assertTrue(result.isFromCache);
    }

    @Test
    public void bitmap_factory_preservesCacheFlagFalse() {
        ImageLoadResult result = ImageLoadResult.bitmap(mock(Drawable.class), false);

        assertFalse(result.isFromCache);
    }

    @Test
    public void bitmap_factory_preservesDrawableReference() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult result = ImageLoadResult.bitmap(drawable, false);

        assertSame(drawable, result.drawable);
    }

    // ──────────────────────────────────────────────────────────────
    // gif() factory
    // ──────────────────────────────────────────────────────────────

    @Test
    public void gif_factory_setsGifFormat() {
        ImageLoadResult result = ImageLoadResult.gif(mock(Drawable.class), false);

        assertEquals(ImageLoadResult.Format.GIF, result.format);
    }

    @Test
    public void gif_factory_preservesCacheFlagTrue() {
        ImageLoadResult result = ImageLoadResult.gif(mock(Drawable.class), true);

        assertTrue(result.isFromCache);
    }

    @Test
    public void gif_factory_preservesDrawableReference() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult result = ImageLoadResult.gif(drawable, true);

        assertSame(drawable, result.drawable);
    }

    // ──────────────────────────────────────────────────────────────
    // Format enum stability (snapshot — guard against accidental rename)
    // ──────────────────────────────────────────────────────────────

    @Test
    public void format_enum_hasExactlyExpectedValues() {
        ImageLoadResult.Format[] values = ImageLoadResult.Format.values();

        assertEquals(5, values.length);
    }

    @Test
    public void format_enum_containsUnknown() {
        assertNotNull(ImageLoadResult.Format.valueOf("UNKNOWN"));
    }

    @Test
    public void format_enum_containsBitmap() {
        assertNotNull(ImageLoadResult.Format.valueOf("BITMAP"));
    }

    @Test
    public void format_enum_containsGif() {
        assertNotNull(ImageLoadResult.Format.valueOf("GIF"));
    }

    @Test
    public void format_enum_containsSvg() {
        assertNotNull(ImageLoadResult.Format.valueOf("SVG"));
    }

    @Test
    public void format_enum_containsLottie() {
        assertNotNull(ImageLoadResult.Format.valueOf("LOTTIE"));
    }

    // ──────────────────────────────────────────────────────────────
    // Round-trip: factory and 3-arg ctor agree
    // ──────────────────────────────────────────────────────────────

    @Test
    public void bitmap_factory_equivalentToThreeArgCtor() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult viaFactory = ImageLoadResult.bitmap(drawable, true);
        ImageLoadResult viaCtor = new ImageLoadResult(
                drawable, true, ImageLoadResult.Format.BITMAP);

        assertSame(viaFactory.drawable, viaCtor.drawable);
        assertEquals(viaFactory.isFromCache, viaCtor.isFromCache);
        assertEquals(viaFactory.format, viaCtor.format);
    }

    @Test
    public void gif_factory_equivalentToThreeArgCtor() {
        Drawable drawable = mock(Drawable.class);

        ImageLoadResult viaFactory = ImageLoadResult.gif(drawable, false);
        ImageLoadResult viaCtor = new ImageLoadResult(
                drawable, false, ImageLoadResult.Format.GIF);

        assertSame(viaFactory.drawable, viaCtor.drawable);
        assertEquals(viaFactory.isFromCache, viaCtor.isFromCache);
        assertEquals(viaFactory.format, viaCtor.format);
    }
}
