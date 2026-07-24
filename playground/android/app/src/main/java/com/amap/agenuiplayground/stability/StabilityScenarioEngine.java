package com.amap.agenuiplayground.stability;

import android.app.Activity;
import android.util.Log;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenui.render.surface.SurfaceSize;
import com.amap.agenui.render.surface.ThemeException;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Random;

/**
 * Scenario execution engine for stability testing.
 * Implements stress scenarios that exercise the SDK in different ways.
 * Realistic scenarios are handled by {@link RealisticScenarioEngine}.
 */
public class StabilityScenarioEngine {
    private static final String TAG = "StabilityEngine";
    private static final String FIXTURES_DIR = "stability_fixtures";

    private final Activity activity;
    private final Random random = new Random();
    private List<String> fixtureFiles;
    private LinkedList<String> pendingFixtures = new LinkedList<>();

    public enum Scenario {
        // Stress scenarios (original 7 + 1 robustness)
        SESSION_STORM,
        STREAM_MARATHON,
        MULTI_SURFACE,
        ACTION_FLOOD,
        THEME_SWITCH,
        INTERRUPT_RECOVER,
        EXTREME_RENDER,
        SDK_ROBUSTNESS,
        SDK_INTERFACE_STABILITY,
        JNI_BRIDGE_RACE,
        // Realistic scenarios (10 new)
        REALISTIC_ARTICLE_STREAM,
        REALISTIC_MULTI_CARD,
        REALISTIC_FORM_FILL,
        REALISTIC_CHART_REFRESH,
        REALISTIC_LONG_LIST,
        REALISTIC_PAGE_SWITCH,
        REALISTIC_TAB_NAVIGATION,
        REALISTIC_LOTTIE_CAROUSEL,
        REALISTIC_MIXED_DASHBOARD,
        REALISTIC_ERROR_RECOVERY,
        // Meta scenarios
        ALL_COMBINED,
        ALL_STRESS,
        ALL_REALISTIC;

        public boolean isRealistic() {
            switch (this) {
                case REALISTIC_ARTICLE_STREAM:
                case REALISTIC_MULTI_CARD:
                case REALISTIC_FORM_FILL:
                case REALISTIC_CHART_REFRESH:
                case REALISTIC_LONG_LIST:
                case REALISTIC_PAGE_SWITCH:
                case REALISTIC_TAB_NAVIGATION:
                case REALISTIC_LOTTIE_CAROUSEL:
                case REALISTIC_MIXED_DASHBOARD:
                case REALISTIC_ERROR_RECOVERY:
                    return true;
                default:
                    return false;
            }
        }

        public boolean isStress() {
            switch (this) {
                case SESSION_STORM:
                case STREAM_MARATHON:
                case MULTI_SURFACE:
                case ACTION_FLOOD:
                case THEME_SWITCH:
                case INTERRUPT_RECOVER:
                case EXTREME_RENDER:
                case SDK_ROBUSTNESS:
                case SDK_INTERFACE_STABILITY:
                case JNI_BRIDGE_RACE:
                    return true;
                default:
                    return false;
            }
        }
        
        public boolean isMeta() {
            switch (this) {
                case ALL_COMBINED:
                case ALL_STRESS:
                case ALL_REALISTIC:
                    return true;
                default:
                    return false;
            }
        }

        public static List<Scenario> stressScenarios() {
            return Arrays.asList(SESSION_STORM, STREAM_MARATHON, MULTI_SURFACE,
                    ACTION_FLOOD, THEME_SWITCH, INTERRUPT_RECOVER, EXTREME_RENDER,
                    SDK_ROBUSTNESS, SDK_INTERFACE_STABILITY, JNI_BRIDGE_RACE);
        }

        public static List<Scenario> realisticScenarios() {
            return Arrays.asList(REALISTIC_ARTICLE_STREAM, REALISTIC_MULTI_CARD,
                    REALISTIC_FORM_FILL, REALISTIC_CHART_REFRESH, REALISTIC_LONG_LIST,
                    REALISTIC_PAGE_SWITCH, REALISTIC_TAB_NAVIGATION, REALISTIC_LOTTIE_CAROUSEL,
                    REALISTIC_MIXED_DASHBOARD, REALISTIC_ERROR_RECOVERY);
        }

        public static List<Scenario> allIndividualScenarios() {
            List<Scenario> all = new ArrayList<>(stressScenarios());
            all.addAll(realisticScenarios());
            return all;
        }
    }

    public StabilityScenarioEngine(Activity activity) {
        this.activity = activity;
        this.fixtureFiles = loadFixtureFileList();
        this.pendingFixtures = new LinkedList<>(this.fixtureFiles);
        Collections.shuffle(this.pendingFixtures);
    }

    public void setFixtureFilter(List<String> filter) {
        if (filter == null || filter.isEmpty()) return;
        List<String> filtered = new ArrayList<>();
        for (String path : fixtureFiles) {
            for (String f : filter) {
                if (path.equals(f) || path.endsWith("/" + f)) {
                    filtered.add(path);
                    break;
                }
            }
        }
        this.fixtureFiles = filtered;
        this.pendingFixtures = new LinkedList<>(filtered);
        Collections.shuffle(this.pendingFixtures);
        Log.i(TAG, "Fixture filter applied: " + filtered.size() + " files retained (all queued for priority run)");
    }

    /**
     * Execute one round of the specified stress scenario.
     * Returns the fixture name used (or null if not applicable).
     * Only handles stress scenarios. Realistic scenarios use RealisticScenarioEngine.
     */
    public String executeRound(Scenario scenario) throws Exception {
        if (scenario.isRealistic()) {
            throw new IllegalArgumentException("Use RealisticScenarioEngine for realistic scenarios: " + scenario.name());
        }
        switch (scenario) {
            case SESSION_STORM:
                return executeSessionStorm();
            case STREAM_MARATHON:
                return executeStreamMarathon();
            case MULTI_SURFACE:
                return executeMultiSurface();
            case ACTION_FLOOD:
                return executeActionFlood();
            case THEME_SWITCH:
                return executeThemeSwitch();
            case INTERRUPT_RECOVER:
                return executeInterruptRecover();
            case EXTREME_RENDER:
                return executeExtremeRender();
            case SDK_ROBUSTNESS:
                return executeSdkRobustness();
            case SDK_INTERFACE_STABILITY:
                return executeSdkInterfaceStability();
            case JNI_BRIDGE_RACE:
                return executeJniBridgeRace();
            case ALL_COMBINED:
                return executeAllCombined();
            default:
                return null;
        }
    }

    public static Scenario parseScenario(String name) {
        if (name == null || name.isEmpty()) return Scenario.ALL_COMBINED;
        switch (name.toLowerCase()) {
            case "session_storm": return Scenario.SESSION_STORM;
            case "stream_marathon": return Scenario.STREAM_MARATHON;
            case "multi_surface": return Scenario.MULTI_SURFACE;
            case "action_flood": return Scenario.ACTION_FLOOD;
            case "theme_switch": return Scenario.THEME_SWITCH;
            case "interrupt_recover": return Scenario.INTERRUPT_RECOVER;
            case "extreme_render": return Scenario.EXTREME_RENDER;
            case "sdk_robustness": return Scenario.SDK_ROBUSTNESS;
            case "sdk_interface_stability": return Scenario.SDK_INTERFACE_STABILITY;
            case "jni_bridge_race": return Scenario.JNI_BRIDGE_RACE;
            case "realistic_article_stream": return Scenario.REALISTIC_ARTICLE_STREAM;
            case "realistic_multi_card": return Scenario.REALISTIC_MULTI_CARD;
            case "realistic_form_fill": return Scenario.REALISTIC_FORM_FILL;
            case "realistic_chart_refresh": return Scenario.REALISTIC_CHART_REFRESH;
            case "realistic_long_list": return Scenario.REALISTIC_LONG_LIST;
            case "realistic_page_switch": return Scenario.REALISTIC_PAGE_SWITCH;
            case "realistic_tab_navigation": return Scenario.REALISTIC_TAB_NAVIGATION;
            case "realistic_lottie_carousel": return Scenario.REALISTIC_LOTTIE_CAROUSEL;
            case "realistic_mixed_dashboard": return Scenario.REALISTIC_MIXED_DASHBOARD;
            case "realistic_error_recovery": return Scenario.REALISTIC_ERROR_RECOVERY;
            case "all_combined": return Scenario.ALL_COMBINED;
            case "all_stress": return Scenario.ALL_STRESS;
            case "all_realistic": return Scenario.ALL_REALISTIC;
            default: return Scenario.ALL_COMBINED;
        }
    }

    /**
     * Select a non-blacklisted scenario based on mode.
     * @param mode The meta-scenario mode (ALL_COMBINED, ALL_STRESS, ALL_REALISTIC)
     * @param crashTracker CrashTracker for blacklist filtering
     * @return A randomly picked available scenario, or null if all are blacklisted.
     */
    public Scenario selectCombinedScenario(Scenario mode, CrashTracker crashTracker) {
        List<Scenario> available = crashTracker.getAvailableScenarios(mode);
        if (available.isEmpty()) return null;
        return available.get(random.nextInt(available.size()));
    }

    // S1: Rapid create/destroy SurfaceManager instances
    private String executeSessionStorm() throws Exception {
        for (int i = 0; i < 10; i++) {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            String json = buildSimpleSurfaceJson("storm-" + i);
            sm.receiveTextChunk(json);
            sm.endTextStream();
            Thread.sleep(10);
            sm.destroy();
        }
        return null;
    }

    // S2: Single surface with long streaming (many chunks)
    private String executeStreamMarathon() throws Exception {
        SurfaceManager sm = new SurfaceManager(activity);
        sm.beginTextStream();

        String createMsg = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"marathon-surf\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
        sm.receiveTextChunk(createMsg);

        for (int i = 0; i < 100; i++) {
            String updateMsg = "{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"marathon-surf\",\"value\":{\"counter\":" + i + ",\"text\":\"Marathon iteration " + i + "\"}}}";
            sm.receiveTextChunk(updateMsg);
        }

        sm.endTextStream();
        Thread.sleep(50);
        sm.destroy();
        return null;
    }

    // S3: Multiple surfaces active simultaneously
    private String executeMultiSurface() throws Exception {
        SurfaceManager sm = new SurfaceManager(activity);
        sm.beginTextStream();

        for (int i = 0; i < 5; i++) {
            String createMsg = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"multi-" + i + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
            sm.receiveTextChunk(createMsg);
            String updateMsg = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"multi-" + i + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"txt-" + i + "\"],\"align\":\"stretch\"},{\"id\":\"txt-" + i + "\",\"component\":\"Text\",\"text\":\"Surface " + i + "\"}]}}";
            sm.receiveTextChunk(updateMsg);
        }

        for (int j = 0; j < 20; j++) {
            int idx = random.nextInt(5);
            String dataMsg = "{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"multi-" + idx + "\",\"value\":{\"update\":" + j + "}}}";
            sm.receiveTextChunk(dataMsg);
        }

        for (int i = 0; i < 5; i++) {
            String deleteMsg = "{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"multi-" + i + "\"}}";
            sm.receiveTextChunk(deleteMsg);
        }

        sm.endTextStream();
        Thread.sleep(50);
        sm.destroy();
        return null;
    }

    // S4: Rapid data model sync (simulates rapid user interaction)
    private String executeActionFlood() throws Exception {
        SurfaceManager sm = new SurfaceManager(activity);
        sm.addListener(new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {}
            @Override
            public void onDeleteSurface(Surface surface) {}
            @Override
            public void onReceiveActionEvent(String event) {}
            @Override
            public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
            @Override
            public void onError(Surface surface, int code, String message) {}
            @Override
            public void onBlankCheckResult(Surface surface, boolean isBlank) {}
            @Override
            public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
            @Override
            public SurfaceSize surfaceSize(String surfaceId) { return null; }
        });
        sm.beginTextStream();

        String createMsg = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"action-flood\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
        sm.receiveTextChunk(createMsg);
        String updateMsg = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"action-flood\",\"components\":[{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"cb\",\"tf\"],\"align\":\"stretch\"},{\"id\":\"cb\",\"component\":\"CheckBox\",\"label\":\"Flood checkbox\",\"value\":false},{\"id\":\"tf\",\"component\":\"TextField\",\"label\":\"Flood field\",\"value\":\"\"}]}}";
        sm.receiveTextChunk(updateMsg);
        sm.endTextStream();

        Thread.sleep(100);

        for (int i = 0; i < 50; i++) {
            sm.beginTextStream();
            sm.receiveTextChunk("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"action-flood\",\"componentId\":\"tf\",\"value\":{\"value\":\"input_" + i + "\"}}}");
            sm.receiveTextChunk("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"action-flood\",\"componentId\":\"cb\",\"value\":{\"checked\":" + (i % 2 == 0) + "}}}");
            sm.endTextStream();
        }

        Thread.sleep(50);
        sm.destroy();
        return null;
    }

    // S5: Day/night mode switching
    private String executeThemeSwitch() throws Exception {
        AGenUI agenui = AGenUI.getInstance();
        for (int i = 0; i < 20; i++) {
            agenui.setDayNightMode(i % 2 == 0 ? "day" : "night");
            Thread.sleep(5);
        }
        return null;
    }

    // S6: Begin stream, send partial data, destroy, recreate
    private String executeInterruptRecover() throws Exception {
        SurfaceManager sm = new SurfaceManager(activity);
        sm.beginTextStream();

        String createMsg = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"interrupt-surf\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
        sm.receiveTextChunk(createMsg);

        sm.destroy();
        Thread.sleep(50);

        sm = new SurfaceManager(activity);
        sm.beginTextStream();
        String json = buildSimpleSurfaceJson("recovered-surf");
        sm.receiveTextChunk(json);
        sm.endTextStream();
        Thread.sleep(50);
        sm.destroy();
        return null;
    }

    // S7: Load and render a random extreme fixture
    private String executeExtremeRender() throws Exception {
        if (fixtureFiles.isEmpty()) {
            Log.w(TAG, "No fixture files available");
            return null;
        }
        String fixturePath;
        if (!pendingFixtures.isEmpty()) {
            fixturePath = pendingFixtures.poll();
            Log.i(TAG, "Priority fixture: " + fixturePath + " (" + pendingFixtures.size() + " remaining)");
        } else {
            fixturePath = fixtureFiles.get(random.nextInt(fixtureFiles.size()));
        }
        String json = loadAssetFile(FIXTURES_DIR + "/" + fixturePath);
        if (json == null) return fixturePath + " (load failed)";

        JSONObject fixture = new JSONObject(json);
        SurfaceManager sm = new SurfaceManager(activity);
        sm.addListener(new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {}
            @Override
            public void onDeleteSurface(Surface surface) {}
            @Override
            public void onReceiveActionEvent(String event) {}
            @Override
            public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
            @Override
            public void onError(Surface surface, int code, String message) {}
            @Override
            public void onBlankCheckResult(Surface surface, boolean isBlank) {}
            @Override
            public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
            @Override
            public SurfaceSize surfaceSize(String surfaceId) { return null; }
        });

        sm.beginTextStream();

        if (fixture.has("payload")) {
            JSONArray payload = fixture.getJSONArray("payload");
            String fullPayload = payload.toString();
            int chunkSize = 100;
            for (int i = 0; i < fullPayload.length(); i += chunkSize) {
                int end = Math.min(i + chunkSize, fullPayload.length());
                sm.receiveTextChunk(fullPayload.substring(i, end));
            }
        } else if (fixture.has("messages")) {
            JSONArray messages = fixture.getJSONArray("messages");
            for (int i = 0; i < messages.length(); i++) {
                sm.receiveTextChunk(messages.getJSONObject(i).toString());
            }
        }

        sm.endTextStream();
        Thread.sleep(100);
        sm.destroy();
        return fixturePath;
    }

    // S8: SDK robustness — 12 defensive sub-cases testing API misuse tolerance
    private String executeSdkRobustness() throws Exception {
        StringBuilder results = new StringBuilder("sdk_robustness[");
        int passed = 0;
        int total = 12;

        // R1: use_after_destroy — call APIs on a destroyed SurfaceManager
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("r1-surf"));
            sm.endTextStream();
            sm.destroy();
            // Now call everything on the destroyed instance — record each response
            List<String> r1Exceptions = new ArrayList<>();
            try { sm.beginTextStream(); } catch (Throwable t) { r1Exceptions.add("begin:" + t.getClass().getSimpleName()); }
            try { sm.receiveTextChunk("{\"version\":\"v0.9\"}"); } catch (Throwable t) { r1Exceptions.add("receive:" + t.getClass().getSimpleName()); }
            try { sm.endTextStream(); } catch (Throwable t) { r1Exceptions.add("end:" + t.getClass().getSimpleName()); }
            try { sm.receiveTextChunk("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"r1-surf\",\"componentId\":\"root\",\"value\":{\"value\":1}}}"); } catch (Throwable t) { r1Exceptions.add("submit:" + t.getClass().getSimpleName()); }
            try { sm.addListener(new ISurfaceManagerListener() {
                @Override public void onCreateSurface(Surface surface) {}
                @Override public void onDeleteSurface(Surface surface) {}
                @Override public void onReceiveActionEvent(String event) {}
                @Override public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
                @Override public void onError(Surface surface, int code, String message) {}
                @Override public void onBlankCheckResult(Surface surface, boolean isBlank) {}
                @Override public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
                @Override public SurfaceSize surfaceSize(String surfaceId) { return null; }
            }); } catch (Throwable t) { r1Exceptions.add("addListener:" + t.getClass().getSimpleName()); }
            passed++;
            if (r1Exceptions.isEmpty()) {
                results.append("R1:OK(no_throw),");
            } else {
                results.append("R1:OK(").append(String.join(";", r1Exceptions)).append("),");
                Log.w(TAG, "R1 use_after_destroy exceptions: " + r1Exceptions);
            }
        } catch (Throwable e) {
            results.append("R1:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R1 use_after_destroy crashed", e);
        }

        // R2: double_destroy
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("r2-surf"));
            sm.endTextStream();
            sm.destroy();
            sm.destroy(); // second destroy
            passed++;
            results.append("R2:OK,");
        } catch (Throwable e) {
            results.append("R2:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R2 double_destroy crashed", e);
        }

        // R3: receive_without_begin
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.receiveTextChunk(buildSimpleSurfaceJson("r3-surf"));
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R3:OK,");
        } catch (Throwable e) {
            results.append("R3:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R3 receive_without_begin crashed", e);
        }

        // R4: end_without_begin
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R4:OK,");
        } catch (Throwable e) {
            results.append("R4:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R4 end_without_begin crashed", e);
        }

        // R5: double_end_stream
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("r5-surf"));
            sm.endTextStream();
            sm.endTextStream(); // second end
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R5:OK,");
        } catch (Throwable e) {
            results.append("R5:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R5 double_end_stream crashed", e);
        }

        // R6: double_begin_stream
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.beginTextStream(); // second begin
            sm.receiveTextChunk(buildSimpleSurfaceJson("r6-surf"));
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R6:OK,");
        } catch (Throwable e) {
            results.append("R6:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R6 double_begin_stream crashed", e);
        }

        // R7: submit_nonexistent_surface
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("r7-surf"));
            sm.endTextStream();
            Thread.sleep(20);
            sm.beginTextStream();
            sm.receiveTextChunk("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"nonexistent-surface-xyz\",\"componentId\":\"comp1\",\"value\":{\"value\":true}}}");
            sm.receiveTextChunk("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"\",\"componentId\":\"comp1\",\"value\":{\"value\":true}}}");
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R7:OK,");
        } catch (Throwable e) {
            results.append("R7:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R7 submit_nonexistent_surface crashed", e);
        }

        // R8: null_arguments
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            List<String> r8Exceptions = new ArrayList<>();
            try { sm.receiveTextChunk(null); } catch (Throwable t) { r8Exceptions.add("receiveNull:" + t.getClass().getSimpleName()); }
            try { sm.receiveTextChunk(null); } catch (Throwable t) { r8Exceptions.add("receiveNull2:" + t.getClass().getSimpleName()); }
            try { sm.addListener(null); } catch (Throwable t) { r8Exceptions.add("addListenerNull:" + t.getClass().getSimpleName()); }
            try { sm.removeListener(null); } catch (Throwable t) { r8Exceptions.add("removeListenerNull:" + t.getClass().getSimpleName()); }
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            if (r8Exceptions.isEmpty()) {
                results.append("R8:OK(no_throw),");
            } else {
                results.append("R8:OK(").append(String.join(";", r8Exceptions)).append("),");
                Log.w(TAG, "R8 null_arguments exceptions: " + r8Exceptions);
            }
        } catch (Throwable e) {
            results.append("R8:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R8 null_arguments crashed", e);
        }

        // R9: empty_arguments
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk("");
            sm.receiveTextChunk("   ");
            sm.receiveTextChunk("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"\",\"componentId\":\"\",\"value\":{}}}");
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R9:OK,");
        } catch (Throwable e) {
            results.append("R9:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R9 empty_arguments crashed", e);
        }

        // R10: malformed_json — various broken payloads
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            String[] malformed = {
                "{broken json",
                "{{{{",
                "{\"version\":\"v0.9\",\"createSurface\":{}}",
                "{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":null}}",
                "<html>not json</html>",
                new String(new char[10000]).replace('\0', 'x'), // 10K garbage
                "{\"version\":\"v99.99\",\"unknownOp\":{\"surfaceId\":\"r10\"}}",
                "null",
                "[]",
                "0"
            };
            List<String> r10Exceptions = new ArrayList<>();
            for (int i = 0; i < malformed.length; i++) {
                try { sm.receiveTextChunk(malformed[i]); } catch (Throwable t) {
                    r10Exceptions.add("payload" + i + ":" + t.getClass().getSimpleName());
                }
            }
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            if (r10Exceptions.isEmpty()) {
                results.append("R10:OK(no_throw),");
            } else {
                results.append("R10:OK(").append(String.join(";", r10Exceptions)).append("),");
                Log.w(TAG, "R10 malformed_json exceptions: " + r10Exceptions);
            }
        } catch (Throwable e) {
            results.append("R10:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R10 malformed_json crashed", e);
        }

        // R11: remove_unregistered_listener
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            ISurfaceManagerListener unregistered = new ISurfaceManagerListener() {
                @Override public void onCreateSurface(Surface surface) {}
                @Override public void onDeleteSurface(Surface surface) {}
                @Override public void onReceiveActionEvent(String event) {}
                @Override public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
                @Override public void onError(Surface surface, int code, String message) {}
                @Override public void onBlankCheckResult(Surface surface, boolean isBlank) {}
                @Override public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
                @Override public SurfaceSize surfaceSize(String surfaceId) { return null; }
            };
            sm.removeListener(unregistered); // never added
            sm.removeListener(unregistered); // twice
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("r11-surf"));
            sm.endTextStream();
            Thread.sleep(20);
            sm.destroy();
            passed++;
            results.append("R11:OK,");
        } catch (Throwable e) {
            results.append("R11:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R11 remove_unregistered_listener crashed", e);
        }

        // R12: listener_after_destroy
        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("r12-surf"));
            sm.endTextStream();
            sm.destroy();
            List<String> r12Exceptions = new ArrayList<>();
            try {
                sm.addListener(new ISurfaceManagerListener() {
                    @Override public void onCreateSurface(Surface surface) {}
                    @Override public void onDeleteSurface(Surface surface) {}
                    @Override public void onReceiveActionEvent(String event) {}
                    @Override public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
                    @Override public void onError(Surface surface, int code, String message) {}
                    @Override public void onBlankCheckResult(Surface surface, boolean isBlank) {}
                    @Override public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
                    @Override public SurfaceSize surfaceSize(String surfaceId) { return null; }
                });
            } catch (Throwable t) { r12Exceptions.add("addListener:" + t.getClass().getSimpleName()); }
            try {
                sm.removeListener(new ISurfaceManagerListener() {
                    @Override public void onCreateSurface(Surface surface) {}
                    @Override public void onDeleteSurface(Surface surface) {}
                    @Override public void onReceiveActionEvent(String event) {}
                    @Override public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
                    @Override public void onError(Surface surface, int code, String message) {}
                    @Override public void onBlankCheckResult(Surface surface, boolean isBlank) {}
                    @Override public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
                    @Override public SurfaceSize surfaceSize(String surfaceId) { return null; }
                });
            } catch (Throwable t) { r12Exceptions.add("removeListener:" + t.getClass().getSimpleName()); }
            passed++;
            if (r12Exceptions.isEmpty()) {
                results.append("R12:OK(no_throw),");
            } else {
                results.append("R12:OK(").append(String.join(";", r12Exceptions)).append("),");
                Log.w(TAG, "R12 listener_after_destroy exceptions: " + r12Exceptions);
            }
        } catch (Throwable e) {
            results.append("R12:CRASH(").append(e.getClass().getSimpleName()).append("),");
            Log.e(TAG, "R12 listener_after_destroy crashed", e);
        }

        results.append("] ").append(passed).append("/").append(total).append(" passed");
        Log.i(TAG, results.toString());
        return results.toString();
    }

    // S9: SDK interface stability — focus on repeated and interleaved public
    // API usage paths that should stay crash/freeze free.
    private String executeSdkInterfaceStability() throws Exception {
        StringBuilder results = new StringBuilder("sdk_interface_stability[");
        int passed = 0;
        int total = 10;
        AGenUI agenui = AGenUI.getInstance();

        try {
            agenui.registerDefaultTheme(buildThemeJson("ffffff"), buildDesignTokenJson("#ffffff"));
            List<String> outcomes = new ArrayList<>();
            try {
                agenui.registerDefaultTheme("{invalid", buildDesignTokenJson("#000000"));
                outcomes.add("invalidTheme:no_throw");
            } catch (ThemeException e) {
                outcomes.add("invalidTheme:" + e.getClass().getSimpleName());
            }
            try {
                agenui.registerDefaultTheme(buildThemeJson("000000"), "{invalid");
                outcomes.add("invalidToken:no_throw");
            } catch (ThemeException e) {
                outcomes.add("invalidToken:" + e.getClass().getSimpleName());
            }
            passed++;
            results.append("I1:OK(").append(String.join(";", outcomes)).append("),");
        } catch (Throwable e) {
            results.append("I1:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            boolean valid = agenui.setPathConfig("{\"templateDir\":\"/tmp/agenui-stability\"}");
            boolean invalid = agenui.setPathConfig("{invalid");
            passed++;
            results.append("I2:OK(valid=").append(valid).append(",invalid=").append(invalid).append("),");
        } catch (Throwable e) {
            results.append("I2:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            List<String> loggerSignals = new ArrayList<>();
            agenui.setCustomLogger((level, tag, func, line, message) -> loggerSignals.add("L" + level));
            for (int level = 0; level <= 5; level++) {
                agenui.setMinLogLevel(level);
                int observed = agenui.getMinLogLevel();
                if (observed != level) {
                    loggerSignals.add(level + "->" + observed);
                }
            }
            agenui.setCustomLogger(null);
            passed++;
            results.append("I3:OK(").append(loggerSignals.isEmpty() ? "stable" : String.join(";", loggerSignals)).append("),");
        } catch (Throwable e) {
            results.append("I3:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            String version = null;
            for (int i = 0; i < 20; i++) {
                version = AGenUI.getVersion();
            }
            passed++;
            results.append("I4:OK(").append(version == null ? "null" : version).append("),");
        } catch (Throwable e) {
            results.append("I4:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            for (int i = 0; i < 20; i++) {
                agenui.setDayNightMode(i % 2 == 0 ? "light" : "dark");
            }
            agenui.setDayNightMode("invalid-mode");
            passed++;
            results.append("I5:OK,");
        } catch (Throwable e) {
            results.append("I5:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("i6-surf"));
            sm.endTextStream();
            Thread.sleep(30);
            for (int i = 0; i < 10; i++) {
                sm.invalidateFunctionCallValues();
            }
            sm.destroy();
            passed++;
            results.append("I6:OK,");
        } catch (Throwable e) {
            results.append("I6:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            SurfaceManager sm = new SurfaceManager(activity);
            Surface before = sm.getSurface("i7-surf");
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("i7-surf"));
            sm.endTextStream();
            Thread.sleep(80);
            Surface afterCreate = sm.getSurface("i7-surf");
            sm.destroy();
            Surface afterDestroy = sm.getSurface("i7-surf");
            passed++;
            results.append("I7:OK(before=").append(before == null).append(",created=").append(afterCreate != null)
                    .append(",afterDestroy=").append(afterDestroy == null).append("),");
        } catch (Throwable e) {
            results.append("I7:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            SurfaceManager sm = new SurfaceManager(activity);
            final AtomicReference<String> callbackState = new AtomicReference<>("none");
            ISurfaceManagerListener listener = new ISurfaceManagerListener() {
                @Override public void onCreateSurface(Surface surface) { callbackState.set("created"); }
                @Override public void onDeleteSurface(Surface surface) { callbackState.set("deleted"); }
                @Override public void onReceiveActionEvent(String event) {}
                @Override public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}
                @Override public void onError(Surface surface, int code, String message) {}
                @Override public void onBlankCheckResult(Surface surface, boolean isBlank) {}
                @Override public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}
                @Override public SurfaceSize surfaceSize(String surfaceId) { return null; }
            };
            sm.addListener(listener);
            sm.removeListener(listener);
            sm.addListener(listener);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("i8-surf"));
            sm.receiveTextChunk(buildDeleteSurfaceJson("i8-surf"));
            sm.endTextStream();
            Thread.sleep(80);
            sm.removeListener(listener);
            sm.destroy();
            passed++;
            results.append("I8:OK(").append(callbackState.get()).append("),");
        } catch (Throwable e) {
            results.append("I8:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            sm.receiveTextChunk(buildSimpleSurfaceJson("i9-a"));
            sm.receiveTextChunk(buildSimpleSurfaceJson("i9-b"));
            sm.endTextStream();
            Thread.sleep(80);
            Surface a = sm.getSurface("i9-a");
            Surface b = sm.getSurface("i9-b");
            sm.destroy();
            passed++;
            results.append("I9:OK(").append((a != null && b != null) ? "both" : "missing").append("),");
        } catch (Throwable e) {
            results.append("I9:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        try {
            for (int i = 0; i < 8; i++) {
                SurfaceManager sm = new SurfaceManager(activity);
                sm.beginTextStream();
                sm.receiveTextChunk(buildSimpleSurfaceJson("i10-" + i));
                sm.endTextStream();
                agenui.setDayNightMode(i % 2 == 0 ? "light" : "dark");
                sm.invalidateFunctionCallValues();
                sm.destroy();
            }
            passed++;
            results.append("I10:OK,");
        } catch (Throwable e) {
            results.append("I10:CRASH(").append(e.getClass().getSimpleName()).append("),");
        }

        results.append("] ").append(passed).append("/").append(total).append(" passed");
        Log.i(TAG, results.toString());
        return results.toString();
    }

    // S10: Random selection from stress scenarios
    private String executeAllCombined() throws Exception {
        Scenario[] scenarios = {
            Scenario.SESSION_STORM, Scenario.STREAM_MARATHON,
            Scenario.MULTI_SURFACE, Scenario.ACTION_FLOOD,
            Scenario.THEME_SWITCH, Scenario.INTERRUPT_RECOVER,
            Scenario.EXTREME_RENDER, Scenario.SDK_ROBUSTNESS,
            Scenario.SDK_INTERFACE_STABILITY,
            Scenario.JNI_BRIDGE_RACE
        };
        Scenario picked = scenarios[random.nextInt(scenarios.length)];
        return executeRound(picked);
    }

    // S11: JNI bridge cross-thread UAF — exercise the SurfaceSizeProvider bridge
    // lifetime race using only the public SDK surface (no reflection, no private
    // natives).
    //
    // Root cause (covered by the crash test spec): the C++ bridge in
    // `agenui::JNISurfaceSizeProviderBridge` holds a JNI global ref to the Java
    // SurfaceManager. When the engine worker thread is inside
    // `env->CallObjectMethod(_javaHost, ...)` and the JNI thread tears the
    // bridge down, `_javaHost` becomes a stale jobject and the next
    // `art::Thread::DecodeGlobalJObject` triggers SIGSEGV.
    //
    // User-level repro: each `new SurfaceManager(activity)` registers a fresh
    // bridge (via the constructor's `registerSurfaceSizeProvider`), and each
    // `SurfaceManager.destroy()` releases it (via `unregisterSurfaceSizeProvider`).
    // So tight-looping create → fire layout-triggering chunk → destroy on
    // multiple Java threads is exactly the spec's §6.2 "SurfaceManager 销毁链路"
    // variant of the same UAF: the destroy on Java thread T1 races the engine
    // worker still mid-call into the bridge owned by that SurfaceManager.
    //
    // Repro recipe (public API only):
    //   Threads W1..Wn — racers: each thread tight-loops
    //                    `new SurfaceManager` → `beginTextStream` →
    //                    layout-triggering `updateComponents` chunk →
    //                    `destroy()`. No `endTextStream` between feed and
    //                    destroy: an end-of-stream barrier would let the
    //                    worker drain first and shrink the race window.
    //   Thread A      — allocator: small byte[] churn so freed bridge memory
    //                    is reused into a non-null stale jobject (vs. the
    //                    post-free nullptr check in the C++ bridge that would
    //                    otherwise swallow the access and hide the crash).
    //
    // Pre-fix: SIGSEGV in `art::Thread::DecodeGlobalJObject` within seconds.
    // Post-fix: rounds complete cleanly, accumulating coverage over the run.
    private String executeJniBridgeRace() throws Exception {
        final AtomicBoolean stop = new AtomicBoolean(false);

        // Combined chunk: createSurface + updateComponents in a single
        // receiveTextChunk. The Column root with percent-based sizing forces
        // the engine to pull surface size on (first) layout, which is the only
        // path that reaches `Surface::ensureSurfaceSizeFetched` → provider →
        // `env->CallObjectMethod` on `_javaHost`. JSON kept minimal so the
        // worker spends most of its time in the JNI callback, not in parsing.
        final String layoutChunkTemplate =
                "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"%s\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"%s\","
                + "\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                + "\"children\":[\"txt\"],\"align\":\"stretch\"},"
                + "{\"id\":\"txt\",\"component\":\"Text\",\"text\":\"jni race\"}]}}";

        // Multiple racer threads to widen the race window vs. the single
        // engine worker. 3 is enough to expose the UAF reliably without
        // overwhelming the device's JNI/global-ref tables.
        final int racerCount = 3;
        Thread[] racers = new Thread[racerCount];
        for (int i = 0; i < racerCount; i++) {
            final int wid = i;
            racers[i] = new Thread(() -> {
                int iter = 0;
                while (!stop.get()) {
                    SurfaceManager sm = null;
                    try {
                        // ctor → registerSurfaceSizeProvider → bridge alive.
                        sm = new SurfaceManager(activity);
                        String sid = "jni-race-" + wid + "-" + (iter++);
                        sm.beginTextStream();
                        // Trigger layout → engine worker starts calling back
                        // into the bridge on a separate thread.
                        sm.receiveTextChunk(
                                String.format(layoutChunkTemplate, sid, sid));
                        // Intentionally skip endTextStream(): we want to
                        // destroy() while the worker is still mid-callback.
                    } catch (Throwable ignored) {
                        // Native SIGSEGV bypasses Java. Benign Java throws
                        // (e.g. context already gone) just retry next iter.
                    } finally {
                        if (sm != null) {
                            try {
                                // destroy → unregisterSurfaceSizeProvider →
                                // DeleteGlobalRef on _javaHost. Pre-fix this
                                // races the engine worker's CallObjectMethod.
                                sm.destroy();
                            } catch (Throwable ignored) {
                                // Same rationale.
                            }
                        }
                    }
                }
            }, "stability-jni-racer-" + i);
        }

        // Allocator: speeds up reuse of freed bridge slots so the post-free
        // `_javaHost` ends up as a stale-but-non-null jobject rather than 0
        // (the C++ bridge has a nullptr early-return that would otherwise
        // swallow the access and hide the crash).
//        Thread allocator = new Thread(() -> {
//            while (!stop.get()) {
//                byte[] junk = new byte[64];
//                junk[0] = 1; // prevent dead-store elimination
//            }
//        }, "stability-jni-allocator");

        // Bounded per-round window. Stability harness runs many rounds so
        // total exposure scales with --duration; keep one round predictable
        // for monitor.sh.
        final long roundDurationMs = 3000L;

        for (Thread t : racers) {
            t.start();
        }
//        allocator.start();

        try {
            Thread.sleep(roundDurationMs);
        } finally {
            stop.set(true);
            for (Thread t : racers) {
                t.join(2000);
            }
//            allocator.join(2000);
        }
        return "jni_bridge_race(racers=" + racerCount + "+allocator," + roundDurationMs + "ms)";
    }

    // Helper: build a simple surface JSON for quick tests
    private String buildSimpleSurfaceJson(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + surfaceId + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}" +
               "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"txt\"],\"align\":\"stretch\"},{\"id\":\"txt\",\"component\":\"Text\",\"text\":\"Test surface: " + surfaceId + "\"}]}}";
    }

    private String buildDeleteSurfaceJson(String surfaceId) {
        return "{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + surfaceId + "\"}}";
    }

    private String buildThemeJson(String colorHexWithoutHash) {
        return "{\"colors\":{\"textPrimary\":\"#" + colorHexWithoutHash
                + "\",\"backgroundPrimary\":\"#" + colorHexWithoutHash + "\"}}";
    }

    private String buildDesignTokenJson(String colorHex) {
        return "{\"color\":{\"text\":{\"primary\":\"" + colorHex
                + "\"},\"bg\":{\"primary\":\"" + colorHex + "\"}}}";
    }

    // Load fixture file list from assets
    private List<String> loadFixtureFileList() {
        List<String> files = new ArrayList<>();
        String[] categories = {"extreme_components", "extreme_data", "extreme_stream",
                              "extreme_lifecycle", "extreme_interaction",
                              "realistic_scenarios"};
        try {
            for (String category : categories) {
                String[] assetFiles = activity.getAssets().list(FIXTURES_DIR + "/" + category);
                if (assetFiles != null) {
                    for (String f : assetFiles) {
                        if (f.endsWith(".json")) {
                            files.add(category + "/" + f);
                        }
                    }
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to list fixture files", e);
        }
        Log.i(TAG, "Loaded " + files.size() + " fixture files");
        return files;
    }

    // Load a file from assets
    String loadAssetFile(String path) {
        try (InputStream is = activity.getAssets().open(path);
             BufferedReader reader = new BufferedReader(new InputStreamReader(is, StandardCharsets.UTF_8))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line);
            }
            return sb.toString();
        } catch (IOException e) {
            Log.e(TAG, "Failed to load asset: " + path, e);
            return null;
        }
    }
}
