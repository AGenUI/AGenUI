package com.amap.agenui.render.component.impl;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/**
 * Custom tab layout that replaces Material TabLayout to reduce package size.
 * Supports fixed/scrollable modes, indicator animation, and text styling.
 */
public class CustomTabLayout extends FrameLayout {

    public static final int MODE_FIXED = 0;
    public static final int MODE_SCROLLABLE = 1;
    public static final int GRAVITY_FILL = 0;

    private static final int INDICATOR_ANIM_DURATION = 250;

    private int tabMode = MODE_FIXED;
    private int tabGravity = GRAVITY_FILL;

    private LinearLayout tabStrip;
    private HorizontalScrollView scrollView;

    private final List<Tab> tabs = new ArrayList<>();
    private int selectedPosition = -1;

    private int normalTextColor = 0xFF666666;
    private int selectedTextColor = 0xFF000000;
    private float tabTextSizePx = 0;
    private float selectedTabTextSizePx = 0;
    private int normalTypeface = android.graphics.Typeface.NORMAL;
    private int selectedTypeface = android.graphics.Typeface.BOLD;

    private Drawable indicatorDrawable;
    private int indicatorHeight = 0;

    private float indicatorLeft = 0;
    private float indicatorRight = 0;
    private ValueAnimator indicatorAnimator;

    private OnTabSelectedListener listener;

    public CustomTabLayout(Context context) {
        super(context);
        init(context);
    }

    private void init(Context context) {
        setWillNotDraw(false);

        tabStrip = new LinearLayout(context);
        tabStrip.setOrientation(LinearLayout.HORIZONTAL);
        tabStrip.setGravity(Gravity.CENTER_VERTICAL);

        scrollView = new HorizontalScrollView(context);
        scrollView.setHorizontalScrollBarEnabled(false);
        scrollView.setOverScrollMode(OVER_SCROLL_NEVER);
        scrollView.setFillViewport(true);
        scrollView.addView(tabStrip, new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));

        addView(scrollView, new LayoutParams(
                LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));
    }

    // ==================== Tab Management ====================

    public Tab newTab() {
        return new Tab(this);
    }

    public void addTab(Tab tab, boolean setSelected) {
        tabs.add(tab);
        addTabView(tab, tabs.size() - 1);
        if (setSelected) {
            selectTab(tab);
        }
    }

    public void removeTabAt(int position) {
        if (position < 0 || position >= tabs.size()) return;
        Tab removed = tabs.remove(position);
        tabStrip.removeViewAt(position);
        if (selectedPosition == position) {
            selectedPosition = -1;
            if (!tabs.isEmpty()) {
                int newPos = Math.min(position, tabs.size() - 1);
                selectTab(tabs.get(newPos));
            }
        } else if (selectedPosition > position) {
            selectedPosition--;
        }
    }

    public Tab getTabAt(int position) {
        if (position < 0 || position >= tabs.size()) return null;
        return tabs.get(position);
    }

    public int getTabCount() {
        return tabs.size();
    }

    // ==================== Configuration ====================

    public void setTabMode(int mode) {
        this.tabMode = mode;
        updateTabStripLayout();
    }

    public void setTabGravity(int gravity) {
        this.tabGravity = gravity;
        updateTabStripLayout();
    }

    public void setTabTextColors(int normalColor, int selectedColor) {
        this.normalTextColor = normalColor;
        this.selectedTextColor = selectedColor;
        updateAllTabStyles();
    }

    public void setTabTextSize(float sizePx) {
        this.tabTextSizePx = sizePx;
        updateAllTabStyles();
    }

    public void setSelectedTabTextSize(float sizePx) {
        this.selectedTabTextSizePx = sizePx;
        updateAllTabStyles();
    }

    public void setTabTextTypeface(int normalStyle, int selectedStyle) {
        this.normalTypeface = normalStyle;
        this.selectedTypeface = selectedStyle;
        updateAllTabStyles();
    }

    public void setSelectedTabIndicator(Drawable indicator) {
        this.indicatorDrawable = indicator;
        invalidate();
    }

    public void setSelectedTabIndicatorHeight(int height) {
        this.indicatorHeight = height;
        invalidate();
    }

    @Override
    public void setMinimumHeight(int minHeight) {
        super.setMinimumHeight(minHeight);
    }

    // ==================== Listener ====================

    public void addOnTabSelectedListener(OnTabSelectedListener listener) {
        this.listener = listener;
    }

    // ==================== Tab Selection ====================

    private void selectTab(Tab tab) {
        int newPosition = tabs.indexOf(tab);
        if (newPosition < 0) return;

        Tab previousTab = selectedPosition >= 0 && selectedPosition < tabs.size()
                ? tabs.get(selectedPosition) : null;

        if (previousTab == tab) {
            if (listener != null) listener.onTabReselected(tab);
            return;
        }

        if (previousTab != null && listener != null) {
            listener.onTabUnselected(previousTab);
        }

        int oldPosition = selectedPosition;
        selectedPosition = newPosition;
        updateAllTabStyles();
        animateIndicator(oldPosition, newPosition);
        scrollToTab(newPosition);

        if (listener != null) {
            listener.onTabSelected(tab);
        }
    }

    // ==================== View Creation ====================

    private void addTabView(Tab tab, int position) {
        Context context = getContext();
        TextView textView = new TextView(context);
        textView.setText(tab.text);
        textView.setGravity(Gravity.CENTER);
        textView.setSingleLine(true);
        textView.setAllCaps(true);

        textView.setBackground(null);

        textView.setOnClickListener(v -> selectTab(tab));
        tab.view = textView;

        LinearLayout.LayoutParams params;
        if (tabMode == MODE_FIXED) {
            params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f);
        } else {
            params = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT);
            int hPadding = (int) (16 * context.getResources().getDisplayMetrics().density);
            textView.setPadding(hPadding, 0, hPadding, 0);
        }
        tabStrip.addView(textView, position, params);

        applyTabStyle(tab, position == selectedPosition);
    }

    private void updateTabStripLayout() {
        for (int i = 0; i < tabStrip.getChildCount(); i++) {
            View child = tabStrip.getChildAt(i);
            LinearLayout.LayoutParams params;
            if (tabMode == MODE_FIXED) {
                params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f);
                child.setPadding(0, 0, 0, 0);
            } else {
                params = new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT);
                int hPadding = (int) (16 * getContext().getResources().getDisplayMetrics().density);
                child.setPadding(hPadding, 0, hPadding, 0);
            }
            child.setLayoutParams(params);
        }

        if (tabMode == MODE_FIXED) {
            tabStrip.setLayoutParams(new FrameLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        } else {
            tabStrip.setLayoutParams(new FrameLayout.LayoutParams(
                    LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
        }
    }

    // ==================== Tab Styling ====================

    private void updateAllTabStyles() {
        for (int i = 0; i < tabs.size(); i++) {
            applyTabStyle(tabs.get(i), i == selectedPosition);
        }
    }

    private void applyTabStyle(Tab tab, boolean selected) {
        if (tab.view == null) return;
        TextView tv = tab.view;
        tv.setTextColor(selected ? selectedTextColor : normalTextColor);
        tv.setTypeface(null, selected ? selectedTypeface : normalTypeface);

        float textSize = selected ? selectedTabTextSizePx : tabTextSizePx;
        if (textSize > 0) {
            tv.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSize);
        }
    }

    // ==================== Indicator ====================

    private void animateIndicator(int fromPosition, int toPosition) {
        if (indicatorAnimator != null) {
            indicatorAnimator.cancel();
        }

        if (fromPosition < 0 || tabStrip.getChildCount() == 0) {
            // First selection or no previous — jump directly
            post(() -> {
                updateIndicatorPosition(toPosition);
                invalidate();
            });
            return;
        }

        View fromTab = tabStrip.getChildAt(fromPosition);
        View toTab = tabStrip.getChildAt(toPosition);
        if (fromTab == null || toTab == null) return;

        final float startLeft = fromTab.getLeft();
        final float startRight = fromTab.getRight();
        final float endLeft = toTab.getLeft();
        final float endRight = toTab.getRight();

        indicatorAnimator = ValueAnimator.ofFloat(0f, 1f);
        indicatorAnimator.setDuration(INDICATOR_ANIM_DURATION);
        indicatorAnimator.setInterpolator(new AccelerateDecelerateInterpolator());
        indicatorAnimator.addUpdateListener(animation -> {
            float fraction = (float) animation.getAnimatedValue();
            indicatorLeft = startLeft + (endLeft - startLeft) * fraction;
            indicatorRight = startRight + (endRight - startRight) * fraction;
            invalidate();
        });
        indicatorAnimator.start();
    }

    private void updateIndicatorPosition(int position) {
        if (position < 0 || position >= tabStrip.getChildCount()) return;
        View tab = tabStrip.getChildAt(position);
        if (tab == null) return;
        indicatorLeft = tab.getLeft();
        indicatorRight = tab.getRight();
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        drawIndicator(canvas);
    }

    private void drawIndicator(Canvas canvas) {
        if (indicatorDrawable == null || indicatorHeight <= 0) return;
        if (indicatorLeft >= indicatorRight) return;

        int scrollX = scrollView.getScrollX();
        int bottom = getHeight();
        int top = bottom - indicatorHeight;

        // Offset by scroll position so indicator aligns with visible tab
        int left = (int) indicatorLeft - scrollX;
        int right = (int) indicatorRight - scrollX;

        indicatorDrawable.setBounds(left, top, right, bottom);
        indicatorDrawable.draw(canvas);
    }

    // ==================== Scroll ====================

    private void scrollToTab(int position) {
        if (tabMode != MODE_SCROLLABLE) return;
        if (position < 0 || position >= tabStrip.getChildCount()) return;

        View tab = tabStrip.getChildAt(position);
        if (tab == null) return;

        int scrollTarget = tab.getLeft() + tab.getWidth() / 2 - scrollView.getWidth() / 2;
        scrollView.smoothScrollTo(Math.max(0, scrollTarget), 0);
    }

    // ==================== Layout Callback ====================

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Force height to minHeight so children (ScrollView) are measured with correct height
        int minH = getMinimumHeight();
        if (minH > 0) {
            heightMeasureSpec = MeasureSpec.makeMeasureSpec(minH, MeasureSpec.EXACTLY);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        // After layout, always sync indicator to current selection (no animation)
        if (selectedPosition >= 0 && (indicatorAnimator == null || !indicatorAnimator.isRunning())) {
            updateIndicatorPosition(selectedPosition);
            invalidate();
        }
    }

    // ==================== Inner Classes ====================

    public static class Tab {
        private final CustomTabLayout parent;
        String text = "";
        TextView view;

        Tab(CustomTabLayout parent) {
            this.parent = parent;
        }

        public void setText(String text) {
            this.text = text != null ? text : "";
            if (view != null) {
                view.setText(this.text);
            }
        }

        public void select() {
            parent.selectTab(this);
        }

        public boolean isSelected() {
            return parent.tabs.indexOf(this) == parent.selectedPosition;
        }

        public int getPosition() {
            return parent.tabs.indexOf(this);
        }

        public View getView() {
            return view;
        }
    }

    public interface OnTabSelectedListener {
        void onTabSelected(Tab tab);
        void onTabUnselected(Tab tab);
        void onTabReselected(Tab tab);
    }
}
