package com.amap.agenuiplayground.tests;

import android.app.Activity;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.A2UIPlaygroundActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * RISK63: tryApplyTextChunk "styles" merge → parseDataBindingDataValue path
 *         type-mismatch → nlohmann::type_error → SIGABRT
 *
 * NEW CODE PATH (introduced in commit 81a445fc, 2026-07-16, within last 15 days):
 *   fix #84070191 "Text流式效果中，对textChunk字段的进行解析处理时同时将styles更新"
 *
 * In core/src/surface/component_manager/agenui_component_manager.cpp
 * ::tryApplyTextChunk() a brand-new branch was added:
 *
 *   if (json.contains("styles")) {                       // key-existence only
 *       auto stylesNode = json["styles"];
 *       auto styleValue = DataValueParser::parseStylesDataValue(
 *               entity.get(), stylesNode.dump());        // ← per-style parse
 *       ...
 *   }
 *
 * parseStylesDataValue() iterates each style value and calls parseDataValue()
 * → parseDataBindingDataValue(), which at line 667 does:
 *
 *   std::string path = json["path"].get<std::string>();  // ← THROWS if non-string
 *
 * There is NO is_string() guard (same root-cause family as RISK40/41/46/47).
 * The updateComponents() loop only catches nlohmann::json::parse_error, NOT
 * type_error, so the exception propagates uncaught → std::terminate() → SIGABRT.
 *
 * ATTACK (SDK public API — receiveTextChunk):
 *   1. Create a Text component "txt1"
 *   2. Send a streaming textChunk update for "txt1" whose "styles" contains a
 *      data-binding object with a non-string "path":
 *          {"id":"txt1","component":"Text","textChunk":"x",
 *           "styles":{"color":{"path":12345}}}
 *
 * This differs from RISK58 (which crashes on the "id" field). RISK63 uses a
 * VALID string id + textChunk and crashes inside the NEW styles-merge branch.
 * Fixing RISK58 does NOT protect this path.
 *
 * Shared core/ code — affects Android, iOS, and HarmonyOS.
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeTextChunkStylesPathTypeMismatchTest {

    private static final String TAG = "RISK63_StylesPath";

    @Rule
    public ActivityTestRule<A2UIPlaygroundActivity> activityRule =
            new ActivityTestRule<>(A2UIPlaygroundActivity.class);

    private Activity activity;

    @Before
    public void setUp() throws Exception {
        activity = activityRule.getActivity();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
        });
        Thread.sleep(300);
    }

    @After
    public void tearDown() {
        // Don't destroy engine — preserve state for crash analysis
    }

    /**
     * Test 1: styles data-binding with "path" as integer.
     *
     * tryApplyTextChunk → parseStylesDataValue → parseDataValue →
     * parseDataBindingDataValue → json["path"].get<std::string>() on 12345
     * → nlohmann::type_error → SIGABRT.
     */
    @Test(timeout = 30000)
    public void test_textChunkStylesPathInteger() throws Exception {
        Log.i(TAG, "=== RISK63 Test 1: textChunk styles path=12345 (integer) ===");
        String surfaceId = "s_r63_t1";
        SurfaceManager sm = createSurfaceAndSetupTextComponent(surfaceId);
        if (sm == null) return;

        // Attack: valid string id + textChunk, but styles.color.path is an integer.
        String attackJson = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"txt1\",\"component\":\"Text\",\"textChunk\":\"world\","
                + "\"styles\":{\"color\":{\"path\":12345}}}"
                + "]}}";

        Log.i(TAG, "Attack: styles.color.path = integer, JSON=" + attackJson);
        sm.beginTextStream();
        sm.receiveTextChunk(attackJson);
        sm.endTextStream();

        Thread.sleep(2000);
        Log.i(TAG, "Test 1 survived (unexpected if bug exists) — process still alive");
    }

    /**
     * Test 2: styles data-binding with "path" as array.
     */
    @Test(timeout = 30000)
    public void test_textChunkStylesPathArray() throws Exception {
        Log.i(TAG, "=== RISK63 Test 2: textChunk styles path=[\"a\",\"b\"] (array) ===");
        String surfaceId = "s_r63_t2";
        SurfaceManager sm = createSurfaceAndSetupTextComponent(surfaceId);
        if (sm == null) return;

        String attackJson = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"txt1\",\"component\":\"Text\",\"textChunk\":\"world\","
                + "\"styles\":{\"fontSize\":{\"path\":[\"a\",\"b\"]}}}"
                + "]}}";

        Log.i(TAG, "Attack: styles.fontSize.path = array");
        sm.beginTextStream();
        sm.receiveTextChunk(attackJson);
        sm.endTextStream();

        Thread.sleep(2000);
        Log.i(TAG, "Test 2 survived (unexpected if bug exists)");
    }

    /**
     * Test 3: styles data-binding with "path" as null.
     */
    @Test(timeout = 30000)
    public void test_textChunkStylesPathNull() throws Exception {
        Log.i(TAG, "=== RISK63 Test 3: textChunk styles path=null ===");
        String surfaceId = "s_r63_t3";
        SurfaceManager sm = createSurfaceAndSetupTextComponent(surfaceId);
        if (sm == null) return;

        String attackJson = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"txt1\",\"component\":\"Text\",\"textChunk\":\"world\","
                + "\"styles\":{\"color\":{\"path\":null}}}"
                + "]}}";

        Log.i(TAG, "Attack: styles.color.path = null");
        sm.beginTextStream();
        sm.receiveTextChunk(attackJson);
        sm.endTextStream();

        Thread.sleep(2000);
        Log.i(TAG, "Test 3 survived (unexpected if bug exists)");
    }

    // ===== Helper =====

    /**
     * Creates a surface and a valid Text component "txt1" so that the follow-up
     * malicious textChunk update reaches tryApplyTextChunk's styles-merge branch.
     */
    private SurfaceManager createSurfaceAndSetupTextComponent(String surfaceId) throws Exception {
        final SurfaceManager sm = new SurfaceManager(activity);
        if (sm == null) {
            Log.e(TAG, "Failed to create SurfaceManager");
            return null;
        }

        String createSurface = "{\"createSurface\":{\"surfaceId\":\"" + surfaceId + "\",\"catalogId\":\"test\"}}";
        sm.beginTextStream();
        sm.receiveTextChunk(createSurface);
        sm.endTextStream();
        Thread.sleep(200);

        // Create a valid Text component so tryApplyTextChunk finds an existing Text.
        String setupJson = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"txt1\",\"component\":\"Text\",\"attributes\":{\"text\":\"\\\"hello\\\"\"}}"
                + "]}}";
        Log.i(TAG, "Setup: creating valid Text component txt1");
        sm.beginTextStream();
        sm.receiveTextChunk(setupJson);
        sm.endTextStream();
        Thread.sleep(500);

        return sm;
    }
}
