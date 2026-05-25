package com.amap.agenui.render.layout;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Hosts children with Yoga-computed absolute positions.
 *
 * Responsibilities:
 * 1. Applies absolute x/y/width/height frames produced by native Yoga.
 * 2. Measures every Yoga child with exact specs to avoid a second Java-side layout system.
 * 3. Maintains drawing order through z-index without re-adding children.
 * 4. Batches child frame updates through {@link #applyYogaResults(List)}.
 */
public class YogaAbsoluteLayout extends ViewGroup {

    /**
     * Immutable Yoga frame snapshot in Android px space.
     */
    public static final class LayoutState {
        public final int xPx;
        public final int yPx;
        public final int widthPx;
        public final int heightPx;
        public final int zIndex;

        public LayoutState(int xPx, int yPx, int widthPx, int heightPx, int zIndex) {
            this.xPx = xPx;
            this.yPx = yPx;
            this.widthPx = widthPx;
            this.heightPx = heightPx;
            this.zIndex = zIndex;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) {
                return true;
            }
            if (!(other instanceof LayoutState)) {
                return false;
            }
            LayoutState that = (LayoutState) other;
            return xPx == that.xPx
                    && yPx == that.yPx
                    && widthPx == that.widthPx
                    && heightPx == that.heightPx
                    && zIndex == that.zIndex;
        }

        @Override
        public int hashCode() {
            int result = xPx;
            result = 31 * result + yPx;
            result = 31 * result + widthPx;
            result = 31 * result + heightPx;
            result = 31 * result + zIndex;
            return result;
        }
    }

    /**
     * One batched child-layout update emitted by SurfaceLayoutDispatcher/A2UIComponent.
     */
    public static final class ChildLayout {
        public final View view;
        public final LayoutState state;

        public ChildLayout(View view, LayoutState state) {
            this.view = view;
            this.state = state;
        }
    }

    /**
     * LayoutParams carrying the resolved Yoga frame for a child view.
     */
    public static class YogaLayoutParams extends MarginLayoutParams {
        public int yogaX;
        public int yogaY;
        public int yogaWidth;
        public int yogaHeight;
        public int zIndex;
        public boolean measureWrapContentHeightWhenZero;

        public YogaLayoutParams(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public YogaLayoutParams(int width, int height) {
            super(width, height);
            yogaWidth = width;
            yogaHeight = height;
        }

        public YogaLayoutParams(LayoutParams source) {
            super(source);
            yogaWidth = source.width;
            yogaHeight = source.height;
        }
    }

    private final List<Integer> drawingOrder = new ArrayList<>();

    public YogaAbsoluteLayout(Context context) {
        super(context);
        init();
    }

    public YogaAbsoluteLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public YogaAbsoluteLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setClipChildren(false);
        setClipToPadding(false);
        setChildrenDrawingOrderEnabled(true);
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new YogaLayoutParams(0, 0);
    }

    @Override
    protected LayoutParams generateLayoutParams(LayoutParams p) {
        return new YogaLayoutParams(p);
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new YogaLayoutParams(getContext(), attrs);
    }

    @Override
    protected boolean checkLayoutParams(LayoutParams p) {
        return p instanceof YogaLayoutParams;
    }

    @Override
    protected int getChildDrawingOrder(int childCount, int drawingPosition) {
        if (drawingOrder.size() != childCount) {
            rebuildDrawingOrder();
        }
        if (drawingPosition < 0 || drawingPosition >= drawingOrder.size()) {
            return drawingPosition;
        }
        return drawingOrder.get(drawingPosition);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        int heightMode = MeasureSpec.getMode(heightMeasureSpec);
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        // Important for scroll containers:
        // YogaAbsoluteLayout may be used as the direct child of HorizontalScrollView/ScrollView.
        // In that role the parent viewport width is only the visible window, while the actual
        // scroll range must come from the furthest Yoga child frame (x + width / y + height).
        // If we only propagated the parent MeasureSpec size, horizontal lists would collapse to
        // the viewport width and Android would conclude there is no extra content to scroll.
        int contentWidth = getPaddingLeft() + getPaddingRight();
        int contentHeight = getPaddingTop() + getPaddingBottom();

        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) {
                continue;
            }

            LayoutParams rawParams = child.getLayoutParams();
            if (rawParams instanceof YogaLayoutParams) {
                YogaLayoutParams params = (YogaLayoutParams) rawParams;
                int childWidthSpec = MeasureSpec.makeMeasureSpec(
                        Math.max(0, params.yogaWidth),
                        MeasureSpec.EXACTLY);
                int childHeightSpec;
                if (params.measureWrapContentHeightWhenZero && params.yogaHeight <= 0) {
                    childHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
                } else {
                    childHeightSpec = MeasureSpec.makeMeasureSpec(
                            Math.max(0, params.yogaHeight),
                            MeasureSpec.EXACTLY);
                }
                child.measure(childWidthSpec, childHeightSpec);
                int effectiveChildHeight = resolveEffectiveChildHeight(child, params);
                contentWidth = Math.max(contentWidth, params.yogaX + params.yogaWidth + getPaddingRight());
                contentHeight = Math.max(
                        contentHeight,
                        params.yogaY + effectiveChildHeight + getPaddingBottom());
            } else {
                measureChild(child, widthMeasureSpec, heightMeasureSpec);
                contentWidth = Math.max(contentWidth,
                        child.getMeasuredWidth() + getPaddingLeft() + getPaddingRight());
                contentHeight = Math.max(contentHeight,
                        child.getMeasuredHeight() + getPaddingTop() + getPaddingBottom());
            }
        }

        int measuredWidth = resolveMeasuredDimension(contentWidth, widthMode, widthSize);
        int measuredHeight = resolveMeasuredDimension(contentHeight, heightMode, heightSize);
        setMeasuredDimension(measuredWidth, measuredHeight);
    }

    private int resolveMeasuredDimension(int desiredSize, int mode, int size) {
        switch (mode) {
            case MeasureSpec.EXACTLY:
                return size;
            case MeasureSpec.AT_MOST:
                return Math.min(desiredSize, size);
            case MeasureSpec.UNSPECIFIED:
            default:
                return desiredSize;
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) {
                continue;
            }

            LayoutParams rawParams = child.getLayoutParams();
            if (rawParams instanceof YogaLayoutParams) {
                YogaLayoutParams params = (YogaLayoutParams) rawParams;
                int childHeight = resolveEffectiveChildHeight(child, params);
                child.layout(
                        params.yogaX,
                        params.yogaY,
                        params.yogaX + params.yogaWidth,
                        params.yogaY + childHeight);
            } else {
                child.layout(0, 0, child.getMeasuredWidth(), child.getMeasuredHeight());
            }
        }
    }

    /**
     * Applies a batch of Yoga frame updates and coalesces them into at most one layout pass.
     */
    public void applyYogaResults(List<ChildLayout> results) {
        boolean layoutChanged = false;
        boolean orderChanged = false;

        for (ChildLayout result : results) {
            if (result == null || result.view == null || result.view.getParent() != this) {
                continue;
            }

            YogaLayoutParams params = ensureYogaLayoutParams(result.view);
            LayoutState state = result.state;
            if (state == null) {
                continue;
            }

            if (params.yogaX != state.xPx || params.yogaY != state.yPx
                    || params.yogaWidth != state.widthPx || params.yogaHeight != state.heightPx) {
                params.yogaX = state.xPx;
                params.yogaY = state.yPx;
                params.yogaWidth = state.widthPx;
                params.yogaHeight = state.heightPx;
                params.width = state.widthPx;
                params.height = state.heightPx;
                layoutChanged = true;
            }

            if (params.zIndex != state.zIndex) {
                params.zIndex = state.zIndex;
                orderChanged = true;
            }
        }

        if (orderChanged) {
            rebuildDrawingOrder();
            invalidate();
        }
        if (layoutChanged || orderChanged) {
            requestLayout();
        }
    }

    /**
     * Mirrors CSS overflow hidden/visible semantics for Yoga containers.
     */
    public void setOverflowHidden(boolean hidden) {
        setClipChildren(hidden);
        setClipToPadding(hidden);
    }

    /**
     * Ensures the child uses YogaLayoutParams so future frame updates stay on the Yoga path.
     */
    private YogaLayoutParams ensureYogaLayoutParams(View child) {
        LayoutParams rawParams = child.getLayoutParams();
        if (rawParams instanceof YogaLayoutParams) {
            return (YogaLayoutParams) rawParams;
        }

        YogaLayoutParams params = rawParams == null
                ? new YogaLayoutParams(0, 0)
                : new YogaLayoutParams(rawParams);
        child.setLayoutParams(params);
        return params;
    }

    /**
     * Rebuilds child drawing order from z-index while keeping document order stable for ties.
     */
    private void rebuildDrawingOrder() {
        int childCount = getChildCount();
        drawingOrder.clear();
        List<Integer> indices = new ArrayList<>(childCount);
        for (int i = 0; i < childCount; i++) {
            indices.add(i);
        }
        Collections.sort(indices, (left, right) -> {
            int zIndexCompare = Integer.compare(getChildZIndex(left), getChildZIndex(right));
            if (zIndexCompare != 0) {
                return zIndexCompare;
            }
            return Integer.compare(left, right);
        });
        drawingOrder.addAll(indices);
    }

    private int getChildZIndex(int index) {
        View child = getChildAt(index);
        if (child == null) {
            return 0;
        }
        LayoutParams params = child.getLayoutParams();
        if (params instanceof YogaLayoutParams) {
            return ((YogaLayoutParams) params).zIndex;
        }
        return 0;
    }

    private int resolveEffectiveChildHeight(View child, YogaLayoutParams params) {
        if (params.measureWrapContentHeightWhenZero && params.yogaHeight <= 0) {
            return Math.max(0, child.getMeasuredHeight());
        }
        return Math.max(0, params.yogaHeight);
    }
}
