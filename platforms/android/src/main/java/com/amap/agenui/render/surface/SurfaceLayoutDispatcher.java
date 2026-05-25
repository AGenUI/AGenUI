package com.amap.agenui.render.surface;

import android.content.Context;
import com.amap.agenui.render.utils.AGenUILogger;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.amap.agenui.render.layout.YogaAbsoluteLayout;
import com.amap.agenui.render.style.StyleHelper;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Batches Yoga layout results and surface callbacks per Surface.
 *
 * It is the Java-side transaction coordinator between NativeEventBridge/Surface and the actual
 * container views: layout frames are queued here first, then flushed once per transaction so
 * large updateComponents batches do not trigger repeated requestLayout() storms.
 */
public final class SurfaceLayoutDispatcher {
    private static final String TAG = "SurfaceLayoutDisp";

    /**
     * Callback sink back into SurfaceManager for native-facing notifications.
     */
    interface Callback {
        void onRenderFinish(String surfaceId,
                            String componentId,
                            String type,
                            float width,
                            float height,
                            int selectedIndex);

        void onSurfaceSizeChanged(String surfaceId, float width, float height);
    }

    private static final class LayoutUpdate {
        final View view;
        final YogaAbsoluteLayout.LayoutState state;
        final boolean measureWrapContentHeightWhenZero;

        LayoutUpdate(View view,
                     YogaAbsoluteLayout.LayoutState state,
                     boolean measureWrapContentHeightWhenZero) {
            this.view = view;
            this.state = state;
            this.measureWrapContentHeightWhenZero = measureWrapContentHeightWhenZero;
        }
    }

    private final String surfaceId;
    private final Callback callback;
    private final Map<YogaAbsoluteLayout, List<YogaAbsoluteLayout.ChildLayout>> batchedYogaUpdates =
            new LinkedHashMap<>();
    private final Map<ViewGroup, List<LayoutUpdate>> batchedDirectUpdates = new LinkedHashMap<>();

    private boolean inTransaction;
    private int lastRootConstraintWidthPx = -1;

    public SurfaceLayoutDispatcher(@NonNull String surfaceId, @NonNull Callback callback) {
        this.surfaceId = surfaceId;
        this.callback = callback;
    }

    /**
     * Starts a layout transaction. Until {@link #endTransaction()} is called, incoming layout
     * updates are buffered instead of being flushed immediately.
     */
    public void beginTransaction() {
        inTransaction = true;
    }

    /**
     * Ends the current layout transaction and flushes all buffered layout updates once.
     */
    public void endTransaction() {
        inTransaction = false;
        flush();
    }

    /**
     * Enqueues one component frame update for the given parent container.
     *
     * YogaAbsoluteLayout parents use the batched absolute-layout path, while non-Yoga parents
     * fall back to a direct MarginLayoutParams update path.
     */
    public void dispatchLayout(@NonNull View view,
                               @Nullable ViewGroup parent,
                               @NonNull YogaAbsoluteLayout.LayoutState state,
                               boolean measureWrapContentHeightWhenZero) {
        ViewGroup targetParent = parent;
        if (targetParent == null && view.getParent() instanceof ViewGroup) {
            targetParent = (ViewGroup) view.getParent();
        }
        if (targetParent == null) {
            return;
        }

        if (targetParent instanceof YogaAbsoluteLayout) {
            YogaAbsoluteLayout container = (YogaAbsoluteLayout) targetParent;
            getOrCreateChildLayouts(container)
                    .add(new YogaAbsoluteLayout.ChildLayout(view, state));
        } else {
            getOrCreateDirectUpdates(targetParent)
                    .add(new LayoutUpdate(view, state, measureWrapContentHeightWhenZero));
        }

        if (!inTransaction) {
            flush();
        }
    }

    /**
     * Reports the external root constraint back to native Yoga only when that constraint changes.
     *
     * Today the native root layout is width-driven: `VirtualDOM::updateSurfaceSize()` eventually
     * calls `YGNodeCalculateLayout(rootWidth, YGUndefined, ...)`, so a height-only change is just
     * a Java-side render result and must not bounce back into another native relayout.
     */
    public void reportSurfaceSize(@NonNull Context context, int widthPx, int heightPx) {
        if (widthPx <= 0) {
            return;
        }
        if (widthPx == lastRootConstraintWidthPx) {
            return;
        }
        lastRootConstraintWidthPx = widthPx;
        callback.onSurfaceSizeChanged(
                surfaceId,
                StyleHelper.pxToA2ui(context, widthPx),
                StyleHelper.pxToA2ui(context, Math.max(heightPx, 0)));
    }

    /**
     * Forwards an async render-finish signal from a component back into the native layout loop.
     */
    public void notifyRenderFinish(@NonNull String componentId,
                                   @NonNull String type,
                                   float width,
                                   float height,
                                   int selectedIndex) {
        callback.onRenderFinish(surfaceId, componentId, type, width, height, selectedIndex);
    }

    /**
     * Flushes all buffered layout updates to the corresponding parent containers.
     */
    private void flush() {
        if (!batchedYogaUpdates.isEmpty()) {
            for (Map.Entry<YogaAbsoluteLayout, List<YogaAbsoluteLayout.ChildLayout>> entry
                    : batchedYogaUpdates.entrySet()) {
                entry.getKey().applyYogaResults(entry.getValue());
            }
            batchedYogaUpdates.clear();
        }

        if (!batchedDirectUpdates.isEmpty()) {
            for (Map.Entry<ViewGroup, List<LayoutUpdate>> entry : batchedDirectUpdates.entrySet()) {
                applyDirectUpdates(entry.getKey(), entry.getValue());
            }
            batchedDirectUpdates.clear();
        }
    }

    /**
     * Backward-compatible replacement for Map.computeIfAbsent on minSdk 21.
     */
    @NonNull
    private List<YogaAbsoluteLayout.ChildLayout> getOrCreateChildLayouts(
            @NonNull YogaAbsoluteLayout container) {
        List<YogaAbsoluteLayout.ChildLayout> layouts = batchedYogaUpdates.get(container);
        if (layouts == null) {
            layouts = new ArrayList<>();
            batchedYogaUpdates.put(container, layouts);
        }
        return layouts;
    }

    /**
     * Backward-compatible replacement for Map.computeIfAbsent on minSdk 21.
     */
    @NonNull
    private List<LayoutUpdate> getOrCreateDirectUpdates(@NonNull ViewGroup parent) {
        List<LayoutUpdate> updates = batchedDirectUpdates.get(parent);
        if (updates == null) {
            updates = new ArrayList<>();
            batchedDirectUpdates.put(parent, updates);
        }
        return updates;
    }

    /**
     * Applies fallback frame updates for non-Yoga parents such as the root FrameLayout.
     */
    private void applyDirectUpdates(@NonNull ViewGroup parent, @NonNull List<LayoutUpdate> updates) {
        boolean parentNeedsLayout = false;
        for (LayoutUpdate update : updates) {
            View view = update.view;
            if (view.getParent() != parent) {
                continue;
            }

            ViewGroup.LayoutParams rawParams = view.getLayoutParams();
            int targetHeight = resolveDirectTargetHeight(update);
            if (!(rawParams instanceof ViewGroup.MarginLayoutParams)) {
                rawParams = new ViewGroup.MarginLayoutParams(
                        update.state.widthPx,
                        targetHeight);
                view.setLayoutParams(rawParams);
            }

            ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) rawParams;
            if (params.width != update.state.widthPx || params.height != targetHeight
                    || params.leftMargin != update.state.xPx || params.topMargin != update.state.yPx) {
                params.width = update.state.widthPx;
                params.height = targetHeight;
                params.leftMargin = update.state.xPx;
                params.topMargin = update.state.yPx;
                parentNeedsLayout = true;
            }

            if (params instanceof FrameLayout.LayoutParams) {
                ((FrameLayout.LayoutParams) params).gravity = Gravity.TOP | Gravity.START;
            }

            view.setZ(update.state.zIndex);
        }

        if (parentNeedsLayout) {
            parent.requestLayout();
        }
    }

    private int resolveDirectTargetHeight(@NonNull LayoutUpdate update) {
        if (update.measureWrapContentHeightWhenZero && update.state.heightPx <= 0) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "use WRAP_CONTENT height for direct update, widthPx=" + update.state.widthPx
                        + ", view=" + update.view.getClass().getSimpleName());
            }
            return ViewGroup.LayoutParams.WRAP_CONTENT;
        }
        return update.state.heightPx;
    }
}
