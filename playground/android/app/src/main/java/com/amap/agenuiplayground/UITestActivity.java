package com.amap.agenuiplayground;

import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.widget.FrameLayout;

import androidx.appcompat.app.AppCompatActivity;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenui.render.surface.SurfaceSize;
import com.amap.agenuiplayground.component.factory.ChartComponentFactory;
import com.amap.agenuiplayground.component.factory.LottieComponentFactory;
import com.amap.agenuiplayground.component.factory.MarkdownComponentFactory;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Lightweight UITest Activity for automated testing.
 *
 * Launched via:
 *   adb shell am start -n com.amap.agenuiplayground/.UITestActivity --es testCase <case_id>
 *
 * The runner pushes test_bundle.json to /data/local/tmp/agenui_test/<case_id>/,
 * then this activity reads and replays the protocol sequence via SurfaceManager.
 */
public class UITestActivity extends AppCompatActivity {
    private static final String TAG = "UITest";
    private SurfaceManager surfaceManager;
    private FrameLayout renderContainer;
    private CountDownLatch surfaceReadyLatch;
    private PlaygroundRuntimeLogger runtimeLogger;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Full-screen, chrome-free render container
        renderContainer = new FrameLayout(this);
        renderContainer.setClipChildren(false);
        renderContainer.setBackgroundColor(Color.WHITE);
        setContentView(renderContainer);

        // Hide action bar to avoid title bar overlap
        if (getSupportActionBar() != null) {
            getSupportActionBar().hide();
        }

        // Read intent extras
        String testCase = getIntent().getStringExtra("testCase");
        String testDataPath = getIntent().getStringExtra("testDataPath");

        // Determine the bundle file path
        String bundlePath;
        if (testDataPath != null && !testDataPath.isEmpty()) {
            bundlePath = testDataPath;
        } else if (testCase != null && !testCase.isEmpty()) {
            bundlePath = "/data/local/tmp/agenui_test/" + testCase + "/" + testCase + "_bundle.json";
        } else {
            Log.e(TAG, "No testCase or testDataPath provided");
            finish();
            return;
        }

        // Initialize AGenUI engine (idempotent)
        AGenUI agenui = AGenUI.getInstance();
        agenui.initialize(getApplicationContext());

        // Setup runtime logger for benchmark timing
        runtimeLogger = new PlaygroundRuntimeLogger(getApplicationContext());
        agenui.setCustomLogger(runtimeLogger);

        // Latch to synchronize: wait for onCreateSurface before sending updateComponents
        surfaceReadyLatch = new CountDownLatch(1);

        // Create SurfaceManager
        surfaceManager = new SurfaceManager(this);
        surfaceManager.addListener(new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                runOnUiThread(() -> {
                    renderContainer.removeAllViews();
                    if (surface != null) {
                        renderContainer.addView(surface.getContainer(),
                                new FrameLayout.LayoutParams(
                                        FrameLayout.LayoutParams.MATCH_PARENT,
                                        FrameLayout.LayoutParams.WRAP_CONTENT,
                                        android.view.Gravity.CENTER));
                    }
                    Log.i(TAG, "Surface ready: " + surface.getSurfaceId());
                    surfaceReadyLatch.countDown();
                });
            }

            @Override
            public void onDeleteSurface(Surface surface) {
                // no-op
            }

            @Override
            public void onReceiveActionEvent(String event) {
            }

            @Override
            public void onRootComponentUpdate(Surface surface, Map<String, String> props) {
            }

            @Override
            public void onError(Surface surface, int code, String message) {
            }

            @Override
            public void onBlankCheckResult(Surface surface, boolean isBlank) {
            }

            @Override
            public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {
            }

            @Override
            public SurfaceSize surfaceSize(String surfaceId) {
                return null;
            }
        });

        // Register custom fonts from assets
        agenui.registerFontFromAsset("Nunito", "fonts/Nunito-Regular.ttf");
        agenui.registerFontFromAsset("PlayfairDisplay", "fonts/PlayfairDisplay-Regular.ttf");
        agenui.registerFontFromAsset("FiraCode", "fonts/FiraCode-Regular.ttf");

        // Register custom components (same as A2UIPlaygroundActivity)
        agenui.registerComponent("Markdown", new MarkdownComponentFactory());
        agenui.registerComponent("Lottie", new LottieComponentFactory());
        agenui.registerComponent("Chart", new ChartComponentFactory());

        // Load and render on background thread to allow latch waiting
        final String finalBundlePath = bundlePath;
        new Thread(() -> loadAndRender(finalBundlePath), "UITest-Loader").start();
    }

    private void loadAndRender(String path) {
        try {
            File file = new File(path);
            if (!file.exists()) {
                Log.e(TAG, "Test bundle not found: " + path);
                return;
            }

            String json = new String(
                    Files.readAllBytes(Paths.get(path)),
                    StandardCharsets.UTF_8);
            JSONObject bundle = new JSONObject(json);
            String version = bundle.getString("version");
            JSONArray sequence = bundle.getJSONArray("sequence");

            // Begin stream session (required for C++ engine to dispatch callbacks)
            surfaceManager.beginTextStream();

            for (int i = 0; i < sequence.length(); i++) {
                JSONObject item = sequence.getJSONObject(i);

                // Handle __pause_ms directive: sleep to allow screenshot capture
                if (item.has("__pause_ms")) {
                    long pauseMs = item.getLong("__pause_ms");
                    Log.i(TAG, "Pausing for " + pauseMs + "ms (between protocol steps)");
                    Thread.sleep(pauseMs);
                    continue;
                }

                JSONObject msg = new JSONObject();
                msg.put("version", version);
                for (Iterator<String> it = item.keys(); it.hasNext(); ) {
                    String key = it.next();
                    msg.put(key, item.get(key));
                }

                String msgStr = msg.toString();
                surfaceManager.receiveTextChunk(msgStr);
            }

            // End stream session (flushes pending callbacks)
            surfaceManager.endTextStream();

            Log.i(TAG, "Rendered " + sequence.length() + " messages from: " + path);
        } catch (Exception e) {
            Log.e(TAG, "Failed to load test data", e);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        surfaceManager.destroy();
        surfaceManager = null;
    }
}
