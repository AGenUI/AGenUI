package com.amap.agenui.render.component.impl.list;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.style.StyleHelper;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Custom RecyclerView.LayoutManager that places items at absolute positions
 * computed by the engine's Yoga layout.
 *
 * Why custom LayoutManager: A2UI flow is "engine computes layout → child views
 * sit at predetermined absolute positions". RV's stock LinearLayoutManager
 * cannot honor pre-computed (x, y) frames, only flex along an axis.
 *
 * Frame source — cached with per-layout-pass invalidation:
 *
 * The LM holds a reference (not a copy) to ListComponent's children list.
 * It reads each child's x/y/width/height from child.properties.styles,
 * converts A2UI units to px, and caches the result in a two-level cache:
 *   Level 1 (cachedPxFrames): the parsed Frame list.
 *   Level 2 (cachedContentBounds): the bounding Rect derived from L1.
 *
 * Cache lifecycle: both levels are invalidated at each precise mutation
 * site — addChild, removeChild, setChildren, setDirection,
 * and shouldApplyChildYogaLayout — rather than
 * blanket-clearing in onMeasure (which would defeat caching on every
 * layout pass). During scroll (where onLayoutChildren runs without
 * onMeasure), the cache stays valid because scroll does not change
 * children's properties.
 *
 * See list-lazy-load-frame-staleness-fix.md for the full diagnostic record.
 */
public class YogaLayoutManager extends RecyclerView.LayoutManager {

    private static final String TAG = "YogaLayoutManager";

    /** Direction axis: matches A2UI ListComponent.direction property. */
    public static final int HORIZONTAL = 0;
    public static final int VERTICAL = 1;

    private int direction = VERTICAL;

    /**
     * Live reference to ListComponent.children. The LM reads each child's
     * yoga frame from child.properties.styles on every measure/layout pass.
     * Set once from ListComponent.onCreateView via {@link #setChildren}.
     */
    @Nullable
    private List<A2UIComponent> children;

    /**
     * Captured at onAttachedToWindow so we can convert A2UI units to px
     * without needing a fresh Context on every layout pass.
     */
    @Nullable
    private Context cachedContext;

    /** Current scroll offset (horizontal or vertical depending on direction). */
    private int scrollOffset = 0;

    // ---- Two-level frame cache ----
    // Level 1: parsed px frames from children's styles (expensive: map lookup +
    //          string parse + unit conversion per child).
    // Level 2: bounding Rect derived from Level 1.
    // Both are computed lazily on first access and invalidated in onMeasure
    // (safety net) and shouldApplyChildYogaLayout (scroll-time accuracy).
    // See class javadoc for details.
    @Nullable private List<Frame> cachedPxFrames;
    @Nullable private Rect cachedContentBounds;

    /**
     * Invalidate both cache levels. Called when the data that feeds frame
     * extraction changes: children list swap, direction change, child
     * property update (from ListComponent.shouldApplyChildYogaLayout).
     */
    public void invalidateFrameCache() {
        cachedPxFrames = null;
        cachedContentBounds = null;
    }

    public void setDirection(int direction) {
        if (this.direction != direction) {
            this.direction = direction;
            invalidateFrameCache();
            requestLayout();
        }
    }

    public int getDirection() {
        return direction;
    }

    /**
     * Bind to ListComponent's live children list. The LM holds the reference
     * (not a copy) so any subsequent additions/removals/yoga updates are
     * visible without an explicit refresh call.
     */
    public void setChildren(@NonNull List<A2UIComponent> children) {
        this.children = children;
        invalidateFrameCache();
        requestLayout();
    }

    @Override
    public void onAttachedToWindow(RecyclerView view) {
        super.onAttachedToWindow(view);
        cachedContext = view.getContext();
    }

    /**
     * A child's resolved px frame plus the metadata needed to mirror
     * YogaAbsoluteLayout's measure/layout behavior (z-index, GONE skipping).
     */
    private static final class Frame {
        /** Resolved px rect (left, top, right, bottom). Empty when invalid. */
        final Rect rect;
        final int zIndex;
        /** Live child reference, used for GONE checks. */
        @NonNull
        final A2UIComponent child;

        Frame(Rect rect, int zIndex, @NonNull A2UIComponent child) {
            this.rect = rect;
            this.zIndex = zIndex;
            this.child = child;
        }

        boolean isEmpty() {
            return rect.isEmpty();
        }
    }

    /**
     * Return the current px frames, using Level-1 cache when available.
     * The cache is invalidated at each mutation site (setChildren, setDirection,
     * shouldApplyChildYogaLayout, addChild, removeChild).
     */
    @NonNull
    private List<Frame> currentPxFrames() {
        if (cachedPxFrames != null) {
            return cachedPxFrames;
        }
        if (children == null || cachedContext == null) {
            return Collections.emptyList();
        }
        List<Frame> out = new ArrayList<>(children.size());
        for (A2UIComponent c : children) {
            out.add(extractFrame(c, cachedContext));
        }
        cachedPxFrames = out;
        return out;
    }

    /**
     * True when this child already has a created view that is GONE — mirrors
     * YogaAbsoluteLayout skipping GONE children in measure/layout. When the view
     * is not yet created (virtualized), we cannot know visibility, so we treat
     * it as visible and rely on the frame.
     */
    private static boolean isGone(@NonNull A2UIComponent child) {
        View v = child.getView();
        return v != null && v.getVisibility() == View.GONE;
    }

    private int effectiveFrameHeight(@NonNull Frame frame) {
        return Math.max(0, frame.rect.height());
    }

    /**
     * Bounding box of all current frames; drives content size. Uses Level-2
     * cache when available (invalidated together with Level-1 in onMeasure).
     *
     * Mirrors YogaAbsoluteLayout.onMeasure: starts from container padding and
     * grows by each visible child's (x + width + paddingRight) / (y +
     * effHeight + paddingBottom). GONE children are skipped.
     */
    @NonNull
    private Rect computeContentBoundsPx() {
        if (cachedContentBounds != null) {
            return cachedContentBounds;
        }
        int contentWidth = getPaddingLeft() + getPaddingRight();
        int contentHeight = getPaddingTop() + getPaddingBottom();
        for (Frame f : currentPxFrames()) {
            if (f.isEmpty()) continue;
            if (isGone(f.child)) continue;
            int right = f.rect.left + f.rect.width() + getPaddingRight();
            int bottom = f.rect.top + effectiveFrameHeight(f) + getPaddingBottom();
            if (right > contentWidth) contentWidth = right;
            if (bottom > contentHeight) contentHeight = bottom;
        }
        cachedContentBounds = new Rect(0, 0, contentWidth, contentHeight);
        return cachedContentBounds;
    }

    @Override
    public RecyclerView.LayoutParams generateDefaultLayoutParams() {
        return new RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    @Override
    public boolean isAutoMeasureEnabled() {
        // We measure ourselves from currentPxFrames(); do not let RV auto-measure
        // by walking children (which would force getViewForPosition during measure
        // pass and break lazy semantics).
        return false;
    }

    @Override
    public void onMeasure(@NonNull RecyclerView.Recycler recycler,
                          @NonNull RecyclerView.State state,
                          int widthSpec, int heightSpec) {
        int widthMode = View.MeasureSpec.getMode(widthSpec);
        int widthSize = View.MeasureSpec.getSize(widthSpec);
        int heightMode = View.MeasureSpec.getMode(heightSpec);
        int heightSize = View.MeasureSpec.getSize(heightSpec);

        Rect bounds = computeContentBoundsPx();
        int measuredWidth = resolveMeasuredDimension(bounds.right, widthMode, widthSize);
        int measuredHeight = resolveMeasuredDimension(bounds.bottom, heightMode, heightSize);
        setMeasuredDimension(measuredWidth, measuredHeight);
    }

    /**
     * Resolve a final dimension from desired content size and the parent spec.
     * Mirrors YogaAbsoluteLayout.resolveMeasuredDimension three-state semantics:
     * EXACTLY → spec size, AT_MOST → min(desired, size), UNSPECIFIED → desired.
     */
    private int resolveMeasuredDimension(int desiredSize, int mode, int size) {
        switch (mode) {
            case View.MeasureSpec.EXACTLY:
                return size;
            case View.MeasureSpec.AT_MOST:
                return Math.min(desiredSize, size);
            case View.MeasureSpec.UNSPECIFIED:
            default:
                return desiredSize;
        }
    }

    @Override
    public void onLayoutChildren(@NonNull RecyclerView.Recycler recycler,
                                 @NonNull RecyclerView.State state) {
        if (state.getItemCount() == 0) {
            removeAndRecycleAllViews(recycler);
            return;
        }

        // Clamp scrollOffset to the valid range before layout. Protects against
        // a stale offset restored via onRestoreInstanceState when the content
        // has since shrunk (fewer/smaller items), which would place the viewport
        // entirely outside the content area and render a blank screen.
        clampScrollOffset();

        // Detach existing children to scrap (they'll be re-fetched by getViewForPosition,
        // hitting the recycler's cache — same component view if same position).
        detachAndScrapAttachedViews(recycler);

        List<Frame> frames = currentPxFrames();
        int itemCount = state.getItemCount();
        int frameCount = frames.size();
        if (itemCount != frameCount) {
            AGenUILogger.w(TAG, "onLayoutChildren: itemCount(" + itemCount
                    + ") != frames.size(" + frameCount
                    + "). children list and adapter may be out of sync.");
        }
        Rect viewport = computeViewportRect();
        int n = Math.min(itemCount, frameCount);
        for (int i = 0; i < n; i++) {
            Frame f = frames.get(i);
            if (f.isEmpty()) continue;
            // Skip GONE children (mirrors YogaAbsoluteLayout).
            if (isGone(f.child)) continue;

            int width = f.rect.width();
            int height = effectiveFrameHeight(f);
            // Viewport intersection uses the effective (wrap-resolved) rect.
            Rect frameRect = new Rect(f.rect.left, f.rect.top,
                    f.rect.left + width, f.rect.top + height);
            if (!Rect.intersects(frameRect, viewport)) continue;

            View v = recycler.getViewForPosition(i);
            addView(v);
            measureChildExactly(v, width, height);
            // Convert absolute frame → screen coords by subtracting scroll offset,
            // and add container padding as the layout origin (mirrors YogaAbsoluteLayout).
            int left = f.rect.left + getPaddingLeft() - (direction == HORIZONTAL ? scrollOffset : 0);
            int top = f.rect.top + getPaddingTop() - (direction == VERTICAL ? scrollOffset : 0);
            layoutDecorated(v, left, top, left + width, top + height);
            // z-index: drive draw order via View.setZ (mirrors SurfaceLayoutDispatcher
            // direct path). Always set — covers recycled views that may carry a
            // stale Z from a previous binding.
            v.setZ(f.zIndex);
        }
    }

    private Rect computeViewportRect() {
        int viewportLeft = direction == HORIZONTAL ? scrollOffset : 0;
        int viewportTop = direction == VERTICAL ? scrollOffset : 0;
        int viewportRight = viewportLeft + getWidth();
        int viewportBottom = viewportTop + getHeight();
        return new Rect(viewportLeft, viewportTop, viewportRight, viewportBottom);
    }

    private void measureChildExactly(View child, int width, int height) {
        child.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
    }

    @Override
    public boolean canScrollHorizontally() {
        return direction == HORIZONTAL && computeContentBoundsPx().right > getWidth();
    }

    @Override
    public boolean canScrollVertically() {
        return direction == VERTICAL && computeContentBoundsPx().bottom > getHeight();
    }

    @Override
    public int scrollHorizontallyBy(int dx, @NonNull RecyclerView.Recycler recycler,
                                    @NonNull RecyclerView.State state) {
        if (direction != HORIZONTAL || dx == 0) return 0;
        int max = Math.max(0, computeContentBoundsPx().right - getWidth());
        int newOffset = Math.max(0, Math.min(scrollOffset + dx, max));
        int actualDx = newOffset - scrollOffset;
        if (actualDx == 0) return 0;
        scrollOffset = newOffset;
        offsetChildrenHorizontal(-actualDx);
        // Re-fill any items that have scrolled into the viewport.
        onLayoutChildren(recycler, state);
        return actualDx;
    }

    @Override
    public int scrollVerticallyBy(int dy, @NonNull RecyclerView.Recycler recycler,
                                  @NonNull RecyclerView.State state) {
        if (direction != VERTICAL || dy == 0) return 0;
        int max = Math.max(0, computeContentBoundsPx().bottom - getHeight());
        int newOffset = Math.max(0, Math.min(scrollOffset + dy, max));
        int actualDy = newOffset - scrollOffset;
        if (actualDy == 0) return 0;
        scrollOffset = newOffset;
        offsetChildrenVertical(-actualDy);
        onLayoutChildren(recycler, state);
        return actualDy;
    }

    @Override
    public void scrollToPosition(int position) {
        List<Frame> frames = currentPxFrames();
        // Guard against both the frames list size and the adapter item count
        // to avoid IndexOutOfBoundsException when the two are briefly out of
        // sync (e.g. children list updated but adapter not yet notified).
        int upperBound = Math.min(frames.size(), getItemCount());
        if (position < 0 || position >= upperBound) return;
        Frame f = frames.get(position);
        if (f.isEmpty()) return;
        scrollOffset = direction == HORIZONTAL ? f.rect.left : f.rect.top;
        // Clamp so the offset never exceeds the scrollable range (same
        // protection as onLayoutChildren and scrollVerticallyBy/scrollHorizontallyBy).
        clampScrollOffset();
        requestLayout();
    }

    @Override
    public int computeHorizontalScrollOffset(@NonNull RecyclerView.State state) {
        return direction == HORIZONTAL ? scrollOffset : 0;
    }

    @Override
    public int computeHorizontalScrollExtent(@NonNull RecyclerView.State state) {
        return direction == HORIZONTAL ? getWidth() : 0;
    }

    @Override
    public int computeHorizontalScrollRange(@NonNull RecyclerView.State state) {
        return direction == HORIZONTAL ? computeContentBoundsPx().right : 0;
    }

    @Override
    public int computeVerticalScrollOffset(@NonNull RecyclerView.State state) {
        return direction == VERTICAL ? scrollOffset : 0;
    }

    @Override
    public int computeVerticalScrollExtent(@NonNull RecyclerView.State state) {
        return direction == VERTICAL ? getHeight() : 0;
    }

    @Override
    public int computeVerticalScrollRange(@NonNull RecyclerView.State state) {
        return direction == VERTICAL ? computeContentBoundsPx().bottom : 0;
    }

    /**
     * Persist scroll offset across detach/reattach cycles (e.g. host RV recycles
     * the row containing this list and later re-binds it). Without this, the
     * inner list resets to scrollOffset=0 every time the row scrolls back into
     * view (AND-P1-1 — but the persistence hook itself is cheap to add now).
     */
    @Override
    public android.os.Parcelable onSaveInstanceState() {
        android.os.Bundle b = new android.os.Bundle();
        b.putInt("scrollOffset", scrollOffset);
        b.putInt("direction", direction);
        return b;
    }

    @Override
    public void onRestoreInstanceState(android.os.Parcelable state) {
        if (state instanceof android.os.Bundle) {
            android.os.Bundle b = (android.os.Bundle) state;
            scrollOffset = b.getInt("scrollOffset", 0);
            direction = b.getInt("direction", VERTICAL);
            requestLayout();
        }
    }

    /**
     * Clamp {@link #scrollOffset} to [0, maxScroll] based on the current
     * content bounds and viewport size. Called at the start of
     * {@link #onLayoutChildren} so that a stale offset (e.g. from
     * {@link #onRestoreInstanceState}) never places the viewport beyond
     * the content area.
     */
    private void clampScrollOffset() {
        Rect bounds = computeContentBoundsPx();
        int maxScroll;
        if (direction == HORIZONTAL) {
            maxScroll = Math.max(0, bounds.right - getWidth());
        } else {
            maxScroll = Math.max(0, bounds.bottom - getHeight());
        }
        scrollOffset = Math.max(0, Math.min(scrollOffset, maxScroll));
    }

    /** Test-friendly accessor. */
    Rect getContentBoundsForDebug() {
        return computeContentBoundsPx();
    }

    // ---- A2UI units → px conversion helpers (moved from ListComponent) ----

    /**
     * Read child.properties.styles.x/y/width/height/z-index (A2UI units),
     * convert to px, and bundle into a {@link Frame}. Mirrors
     * A2UIComponent.buildYogaLayout. The rect is empty when width<=0.
     */
    @NonNull
    private static Frame extractFrame(@NonNull A2UIComponent child, @NonNull Context context) {
        Map<String, Object> props = child.getProperties();
        Object stylesObj = props.get("styles");
        if (!(stylesObj instanceof Map)) {
            return new Frame(new Rect(), 0, child);
        }
        Map<?, ?> styles = (Map<?, ?>) stylesObj;
        int x = readIntStyle(styles.get("x"));
        int y = readIntStyle(styles.get("y"));
        int w = readIntStyle(styles.get("width"));
        int h = readIntStyle(styles.get("height"));
        int zIndex = readIntStyle(styles.containsKey("z-index")
                ? styles.get("z-index") : styles.get("zIndex"));
        if (w <= 0) {
            return new Frame(new Rect(), zIndex, child);
        }
        int xPx = StyleHelper.standardUnitToPx(context, x);
        int yPx = StyleHelper.standardUnitToPx(context, y);
        int wPx = Math.max(0, StyleHelper.standardUnitToPx(context, w));
        int hPx = Math.max(0, StyleHelper.standardUnitToPx(context, h));
        Rect rect = new Rect(xPx, yPx, xPx + wPx, yPx + hPx);
        return new Frame(rect, zIndex, child);
    }

    private static int readIntStyle(Object v) {
        if (v instanceof Number) return ((Number) v).intValue();
        if (v == null) return 0;
        try {
            String s = String.valueOf(v).trim();
            if (s.endsWith("px")) s = s.substring(0, s.length() - 2);
            return (int) Float.parseFloat(s);
        } catch (NumberFormatException e) {
            return 0;
        }
    }
}
