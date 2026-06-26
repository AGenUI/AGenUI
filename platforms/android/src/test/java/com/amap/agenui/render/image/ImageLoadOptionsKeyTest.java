package com.amap.agenui.render.image;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Test;

import java.lang.reflect.Constructor;
import java.lang.reflect.Modifier;

/**
 * Unit tests for {@link ImageLoadOptionsKey}.
 *
 * <p>This class is a contract surface exposed to host apps — its
 * <b>string values</b> must remain stable across releases. These
 * snapshot tests guard against accidental renames that would silently
 * break custom loaders.
 *
 * <p>Pure logic — no Android framework dependency.
 */
public class ImageLoadOptionsKeyTest {

    // ──────────────────────────────────────────────────────────────
    // Snapshot: stable string values (host app contract)
    // ──────────────────────────────────────────────────────────────

    @Test
    public void widthKey_hasStableValue() {
        assertEquals("width", ImageLoadOptionsKey.WIDTH);
    }

    @Test
    public void heightKey_hasStableValue() {
        assertEquals("height", ImageLoadOptionsKey.HEIGHT);
    }

    @Test
    public void componentIdKey_hasStableValue() {
        assertEquals("componentId", ImageLoadOptionsKey.COMPONENT_ID);
    }

    @Test
    public void surfaceIdKey_hasStableValue() {
        assertEquals("surfaceId", ImageLoadOptionsKey.SURFACE_ID);
    }

    // ──────────────────────────────────────────────────────────────
    // Uniqueness: every key is distinct
    // ──────────────────────────────────────────────────────────────

    @Test
    public void allKeys_areDistinct() {
        String[] keys = {
                ImageLoadOptionsKey.WIDTH,
                ImageLoadOptionsKey.HEIGHT,
                ImageLoadOptionsKey.COMPONENT_ID,
                ImageLoadOptionsKey.SURFACE_ID
        };
        for (int i = 0; i < keys.length; i++) {
            for (int j = i + 1; j < keys.length; j++) {
                assertNotEquals(
                        "Keys at " + i + " and " + j + " collide: " + keys[i],
                        keys[i], keys[j]);
            }
        }
    }

    // ──────────────────────────────────────────────────────────────
    // Non-null & non-empty: keys never become null / blank
    // ──────────────────────────────────────────────────────────────

    @Test
    public void widthKey_isNotNullOrEmpty() {
        assertNotNull(ImageLoadOptionsKey.WIDTH);
        assertFalse(ImageLoadOptionsKey.WIDTH.isEmpty());
    }

    @Test
    public void heightKey_isNotNullOrEmpty() {
        assertNotNull(ImageLoadOptionsKey.HEIGHT);
        assertFalse(ImageLoadOptionsKey.HEIGHT.isEmpty());
    }

    @Test
    public void componentIdKey_isNotNullOrEmpty() {
        assertNotNull(ImageLoadOptionsKey.COMPONENT_ID);
        assertFalse(ImageLoadOptionsKey.COMPONENT_ID.isEmpty());
    }

    @Test
    public void surfaceIdKey_isNotNullOrEmpty() {
        assertNotNull(ImageLoadOptionsKey.SURFACE_ID);
        assertFalse(ImageLoadOptionsKey.SURFACE_ID.isEmpty());
    }

    // ──────────────────────────────────────────────────────────────
    // Class-level invariant: utility class is final and not instantiable
    // ──────────────────────────────────────────────────────────────

    @Test
    public void class_isFinal() {
        assertTrue(
                "ImageLoadOptionsKey must be final to prevent subclassing",
                Modifier.isFinal(ImageLoadOptionsKey.class.getModifiers()));
    }

    @Test
    public void constructor_isPrivate() throws NoSuchMethodException {
        Constructor<ImageLoadOptionsKey> ctor =
                ImageLoadOptionsKey.class.getDeclaredConstructor();

        assertTrue(
                "Constructor must be private to enforce non-instantiability",
                Modifier.isPrivate(ctor.getModifiers()));
    }

    @Test
    public void instantiation_viaReflection_isStillPossibleButDiscouraged() throws Exception {
        // We don't *prevent* reflection-based instantiation — just confirm
        // it requires bypassing the access modifier, which is the enforced
        // gatekeeper.
        Constructor<ImageLoadOptionsKey> ctor =
                ImageLoadOptionsKey.class.getDeclaredConstructor();
        try {
            ctor.newInstance();
            fail("Expected IllegalAccessException for private constructor");
        } catch (IllegalAccessException expected) {
            // Pass — private modifier blocks default access.
        }
    }
}
