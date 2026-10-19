package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.assertTrue;

/**
 * SDK Risk Probe: Protocol fuzzing — malformed, random, and extreme
 * input data fed through the public streaming API.
 *
 * Hypothesis:
 * The native ProtocolStreamExtractor has a complex hand-written state machine
 * (Idle / StreamingComponents / StreamingDataModel), with manual JSON completeness
 * detection (brace depth tracking, string escape handling), markdown field streaming,
 * and a 10MB buffer limit. Malformed or extreme input may trigger:
 *   - Out-of-bounds access in the character-by-character scanner
 *   - Infinite loop in incomplete JSON detection
 *   - State corruption from unexpected transitions
 *   - Buffer overrun near the 10MB boundary
 *   - Crash from null bytes or binary data in string operations
 *
 * Probe style: boundary test + abnormal input + pressure
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeProtocolFuzzTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe14";
    private static final Random RNG = new Random(0xDEADBEEF);

    @Override
    public void setUp() {
        activityRule.getScenario().onActivity(activity -> {
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
        });
    }

    @Override
    public void tearDown() {
        // No-op: each test manages its own SM lifecycle
    }

    // ===================== RISK14: Malformed data fuzzing =====================

    /**
     * RISK14: Feed a variety of malformed data through receiveTextChunk.
     *
     * Variants tested (100 cycles each, 8 variants = 800 calls):
     *   1. Pure random binary bytes
     *   2. Random ASCII printable chars
     *   3. Data with embedded null bytes
     *   4. Truncated valid JSON at random positions
     *   5. Deeply nested braces (depth 5000)
     *   6. Unbalanced braces / brackets
     *   7. Malformed escape sequences (trailing backslash, broken unicode)
     *   8. Mixed valid protocol frames with garbage interleaved
     *
     * Success = no native crash, no ANR, process stays alive.
     */
    @Test(timeout = 120_000)
    public void testSDKRISK14_protocolFuzzMalformedData() throws Exception {
        final int CYCLES_PER_VARIANT = 100;
        int crashCount = 0;
        int totalChunks = 0;

        // --- Variant 1: Pure random binary ---
        Log.i(TAG, "=== V1: Random binary bytes ===");
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                byte[] randomBytes = new byte[256 + RNG.nextInt(4096)];
                RNG.nextBytes(randomBytes);
                sm.receiveTextChunk(new String(randomBytes));
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V1 caught: " + e.getClass().getSimpleName() + ": " + e.getMessage());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V1 complete: " + CYCLES_PER_VARIANT + " cycles, crashes=" + crashCount);

        // --- Variant 2: Random ASCII printable ---
        Log.i(TAG, "=== V2: Random ASCII printable ===");
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                sm.receiveTextChunk(randomAscii(512 + RNG.nextInt(2048)));
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V2 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V2 complete");

        // --- Variant 3: Embedded null bytes ---
        Log.i(TAG, "=== V3: Embedded null bytes ===");
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                String payload = buildNullBytePayload(i);
                sm.receiveTextChunk(payload);
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V3 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V3 complete");

        // --- Variant 4: Truncated valid JSON ---
        Log.i(TAG, "=== V4: Truncated JSON ===");
        String validJson = buildValidProtocolJson("fuzz-v4");
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                int cutPoint = RNG.nextInt(Math.max(1, validJson.length()));
                sm.receiveTextChunk(validJson.substring(0, cutPoint));
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V4 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V4 complete");

        // --- Variant 5: Deeply nested braces ---
        Log.i(TAG, "=== V5: Deep nested braces (depth 5000) ===");
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                int depth = 1000 + RNG.nextInt(4000);
                sm.receiveTextChunk(buildDeepNested(depth));
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V5 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V5 complete");

        // --- Variant 6: Unbalanced braces/brackets ---
        Log.i(TAG, "=== V6: Unbalanced braces ===");
        String[] unbalanced = {
                "{{{{{{", "}}}}}}", "[[[[[[", "]]]]]]",
                "{\"a\":[[[", "}}}}]]]", "{\"x\":", "{\"x\":\"val",
                "{\"version\":\"v0.9\",\"createSurface\":{{{",
                "{\"version\":\"v0.9\",\"updateComponents\":]]]",
                "}{}{}{", "[]{}[]{", "\"{\\\"", "\\\\\\\\\\",
        };
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                sm.receiveTextChunk(unbalanced[i % unbalanced.length]);
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V6 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V6 complete");

        // --- Variant 7: Malformed escape sequences ---
        Log.i(TAG, "=== V7: Malformed escapes ===");
        String[] badEscapes = {
                "{\"key\":\"value\\\",\"other\":1}",            // trailing backslash in value
                "{\"key\":\"\\u\"}",                              // incomplete unicode
                "{\"key\":\"\\u00\"}",                            // partial unicode
                "{\"key\":\"\\uZZZZ\"}",                         // invalid hex
                "{\"key\":\"\\x41\"}",                            // invalid JSON escape
                "{\"key\":\"\\",                                   // open string with trailing backslash
                "{\"key\":\"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"}", // many escaped backslashes
                "{\"key\":\"" + repeatStr("\\\"", 500) + "\"}",   // 500 escaped quotes
                "{\"key\":\"a\\\",\"b\":\"c\"}",                 // backslash before closing quote
        };
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                sm.receiveTextChunk(badEscapes[i % badEscapes.length]);
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V7 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V7 complete");

        // --- Variant 8: Valid frames interleaved with garbage ---
        Log.i(TAG, "=== V8: Mixed valid + garbage ===");
        for (int i = 0; i < CYCLES_PER_VARIANT; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                // Send valid frame
                sm.receiveTextChunk(buildValidProtocolJson("fuzz-v8-" + i));
                // Immediately follow with garbage
                sm.receiveTextChunk(randomAscii(256));
                // Another valid frame
                sm.receiveTextChunk(buildValidProtocolJson("fuzz-v8b-" + i));
                sm.endTextStream();
                totalChunks++;
            } catch (Exception e) {
                Log.w(TAG, "V8 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "V8 complete");

        Log.i(TAG, "=== RISK14 SUMMARY: total chunks=" + totalChunks
                + ", java exceptions=" + crashCount + " ===");
        // If we reached here, no native crash occurred.
        assertTrue("Process survived all malformed data variants", true);
    }

    // ===================== RISK15: State transition stress =====================

    /**
     * RISK15: Rapid and invalid state transitions on the streaming API.
     *
     * Tests:
     *   A. begin → begin (double begin without end)
     *   B. end without begin
     *   C. receive without begin
     *   D. begin → end (empty stream)
     *   E. begin → receive → receive → ... → end (many chunks, single stream)
     *   F. Rapid create/destroy SM while streaming
     *   G. Large chunk near 10MB buffer limit
     *   H. Extremely rapid begin/end cycles (10000 iterations)
     *
     * Success = no native crash, no freeze (timeout enforced).
     */
    @Test(timeout = 120_000)
    public void testSDKRISK15_stateTransitionStress() throws Exception {
        int crashCount = 0;

        // --- A: Double begin ---
        Log.i(TAG, "=== A: Double begin ===");
        for (int i = 0; i < 50; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                sm.beginTextStream(); // second begin without end
                sm.receiveTextChunk(buildValidProtocolJson("dbl-begin-" + i));
                sm.endTextStream();
            } catch (Exception e) {
                Log.w(TAG, "A caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "A complete");

        // --- B: End without begin ---
        Log.i(TAG, "=== B: End without begin ===");
        for (int i = 0; i < 50; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.endTextStream(); // no begin
            } catch (Exception e) {
                Log.w(TAG, "B caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "B complete");

        // --- C: Receive without begin ---
        Log.i(TAG, "=== C: Receive without begin ===");
        for (int i = 0; i < 50; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.receiveTextChunk(buildValidProtocolJson("no-begin-" + i));
            } catch (Exception e) {
                Log.w(TAG, "C caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "C complete");

        // --- D: Begin → End (empty stream) ---
        Log.i(TAG, "=== D: Empty stream ===");
        for (int i = 0; i < 200; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                sm.endTextStream();
            } catch (Exception e) {
                Log.w(TAG, "D caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "D complete");

        // --- E: Many chunks in single stream ---
        Log.i(TAG, "=== E: Many small chunks ===");
        {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                // Split a valid JSON into single-character chunks
                String payload = buildValidProtocolJson("many-chunks");
                for (int i = 0; i < payload.length(); i++) {
                    sm.receiveTextChunk(String.valueOf(payload.charAt(i)));
                }
                sm.endTextStream();
            } catch (Exception e) {
                Log.w(TAG, "E caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        // Also try with completely random single-byte chunks
        {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                for (int i = 0; i < 500; i++) {
                    sm.receiveTextChunk(String.valueOf((char) (RNG.nextInt(128))));
                }
                sm.endTextStream();
            } catch (Exception e) {
                Log.w(TAG, "E2 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "E complete");

        // --- F: Destroy while streaming (no wait) ---
        Log.i(TAG, "=== F: Destroy mid-stream ===");
        for (int i = 0; i < 100; i++) {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                sm.receiveTextChunk(buildValidProtocolJson("mid-destroy-" + i));
                // Do NOT call endTextStream — destroy immediately
            } catch (Exception e) {
                Log.w(TAG, "F caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "F complete");

        // --- G: Large chunk near 10MB limit ---
        Log.i(TAG, "=== G: Near 10MB buffer ===");
        {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                // Build a ~9.5MB string: valid JSON prefix + huge text field
                StringBuilder huge = new StringBuilder(10 * 1024 * 1024);
                huge.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"big\",\"components\":[{\"id\":\"root\",\"component\":\"Text\",\"text\":\"");
                int targetSize = 9 * 1024 * 1024 + 512 * 1024; // ~9.5MB
                while (huge.length() < targetSize) {
                    huge.append("ABCDEFGHIJ"); // safe ASCII
                }
                huge.append("\"}]}}");
                sm.receiveTextChunk(huge.toString());
                sm.endTextStream();
                Log.i(TAG, "G sent " + (huge.length() / 1024 / 1024) + " MB chunk");
            } catch (Exception e) {
                Log.w(TAG, "G caught: " + e.getClass().getSimpleName() + ": " + e.getMessage());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        // Also try exactly at and beyond 10MB
        {
            SurfaceManager sm = createSM();
            try {
                sm.beginTextStream();
                StringBuilder over = new StringBuilder(11 * 1024 * 1024);
                while (over.length() < 10 * 1024 * 1024 + 100) {
                    over.append("X");
                }
                sm.receiveTextChunk(over.toString());
                sm.endTextStream();
                Log.i(TAG, "G2 sent >10MB chunk");
            } catch (Exception e) {
                Log.w(TAG, "G2 caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "G complete");

        // --- H: Rapid begin/end cycles ---
        Log.i(TAG, "=== H: 10000 rapid begin/end cycles ===");
        {
            SurfaceManager sm = createSM();
            try {
                for (int i = 0; i < 10000; i++) {
                    sm.beginTextStream();
                    sm.endTextStream();
                }
            } catch (Exception e) {
                Log.w(TAG, "H caught: " + e.getClass().getSimpleName());
                crashCount++;
            } finally {
                destroySM(sm);
            }
        }
        Log.i(TAG, "H complete: 10000 rapid cycles");

        Log.i(TAG, "=== RISK15 SUMMARY: java exceptions=" + crashCount + " ===");
        assertTrue("Process survived all state transition stress tests", true);
    }

    // ===================== Helpers =====================

    private SurfaceManager createSM() throws Exception {
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch latch = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            latch.countDown();
        });
        assertTrue("SM creation timeout", latch.await(5, TimeUnit.SECONDS));
        return holder[0];
    }

    private void destroySM(SurfaceManager sm) throws Exception {
        if (sm == null) return;
        final CountDownLatch latch = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            latch.countDown();
        });
        latch.await(5, TimeUnit.SECONDS);
    }

    private static String buildValidProtocolJson(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                + surfaceId + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t1\"],"
                + "\"align\":\"stretch\",\"styles\":{\"width\":\"100%\"}},"
                + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"Fuzz test\"}"
                + "]}}";
    }

    private static String buildNullBytePayload(int variant) {
        String base = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"null-test\"}}";
        char[] chars = base.toCharArray();
        // Insert null bytes at various positions
        int insertPos = variant % chars.length;
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < chars.length; i++) {
            if (i == insertPos) sb.append('\0');
            sb.append(chars[i]);
            if (i == chars.length / 2) sb.append("\0\0\0");
        }
        return sb.toString();
    }

    private static String buildDeepNested(int depth) {
        StringBuilder sb = new StringBuilder(depth * 10);
        for (int i = 0; i < depth; i++) {
            sb.append("{\"d").append(i).append("\":");
        }
        sb.append("\"leaf\"");
        for (int i = 0; i < depth; i++) {
            sb.append("}");
        }
        return sb.toString();
    }

    private static String randomAscii(int length) {
        char[] buf = new char[length];
        for (int i = 0; i < length; i++) {
            buf[i] = (char) (32 + RNG.nextInt(95)); // printable ASCII
        }
        return new String(buf);
    }

    private static String repeatStr(String s, int count) {
        StringBuilder sb = new StringBuilder(s.length() * count);
        for (int i = 0; i < count; i++) {
            sb.append(s);
        }
        return sb.toString();
    }
}
