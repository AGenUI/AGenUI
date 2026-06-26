package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.A2UILayoutComponent;
import com.amap.agenui.render.style.ComponentStyleConfig;
import com.amap.agenui.render.style.StyleHelper;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Tabs component implementation
 *
 * Corresponds to the Tabs component in the A2UI protocol
 * Uses CustomTabLayout to implement tab switching
 *
 * Supported properties:
 * - tabs: tab array (List<Map>) - defined by protocol
 *   - title: tab label text (String) - defined by protocol
 *   - child: content component reference (String) - defined by protocol
 *
 */
public class TabsComponent extends A2UILayoutComponent {

    private static final String TAG = "TabsComponent";
    private static final String RENDER_FINISH_TYPE_TABS_INDEX_CHANGE = "TabsIndexChange";

    private LinearLayout containerLayout;
    private CustomTabLayout tabLayout;
    private FrameLayout contentContainer;
    private List<A2UIComponent> tabContents;
    private int selectedIndex = 0;

    public TabsComponent(String id, Map<String, Object> properties) {
        super(id, "Tabs");

        this.tabContents = new ArrayList<>();
        if (properties != null) {
            this.properties.putAll(properties);
            if (!properties.containsKey("tabs") && AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "[TabsComponent] NO 'tabs' property found");
            }
        }
    }

    @Override
    public View onCreateView(Context context) {
        // 1. Create main container
        containerLayout = createMainContainer(context);

        // 2. Load style configuration
        ComponentStyleConfig.StyleHashMap<String, String> styleConfig = loadStyleConfig(context);

        // 3. Create and configure CustomTabLayout
        tabLayout = createTabLayout(context, styleConfig);
        applyIndicatorStyle(tabLayout, context, styleConfig);
        applyTextStyle(tabLayout, styleConfig);

        // 4. Add TabLayout to container
        containerLayout.addView(tabLayout);

        // 5. Create content container
        contentContainer = createContentContainer(context);
        containerLayout.addView(contentContainer);

        // 6. Set up tab selection listener
        setupTabSelectionListener();

        // 7. Parse tabs configuration
        if (properties.containsKey("tabs")) {
            parseTabs();
        }

        return containerLayout;
    }

    /**
     * Create main container
     */
    private LinearLayout createMainContainer(Context context) {
        LinearLayout container = new LinearLayout(context);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        ));
        return container;
    }

    /**
     * Load style configuration
     */
    private ComponentStyleConfig.StyleHashMap<String, String> loadStyleConfig(Context context) {
        ComponentStyleConfig.StyleHashMap<String, String> styleConfig = ComponentStyleConfig.getInstance(context).getComponentStyle("Tabs");
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "loadStyleConfig: " + styleConfig);
        }
        return styleConfig;
    }

    /**
     * Create and configure CustomTabLayout
     */
    private CustomTabLayout createTabLayout(Context context,
                                            ComponentStyleConfig.StyleHashMap<String, String> styleConfig) {
        CustomTabLayout layout = new CustomTabLayout(context);

        // Apply tab-mode configuration
        String tabMode = styleConfig.getOrDefault("tab-mode", "fixed");
        if ("scrollable".equals(tabMode)) {
            layout.setTabMode(CustomTabLayout.MODE_SCROLLABLE);
        } else {
            layout.setTabMode(CustomTabLayout.MODE_FIXED);
        }
        layout.setTabGravity(CustomTabLayout.GRAVITY_FILL);

        // Set layout parameters
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        layout.setMinimumHeight((int) (48 * context.getResources().getDisplayMetrics().density));
        layout.setLayoutParams(params);

        // Apply text sizes directly (no reflection needed)
        String fontSize = styleConfig.getOrDefault("tab-font-size", "32px");
        layout.setTabTextSize(StyleHelper.parseDimension(fontSize, context));

        String fontSizeSelected = styleConfig.getOrDefault("tab-font-size-selected", "32px");
        layout.setSelectedTabTextSize(StyleHelper.parseDimension(fontSizeSelected, context));

        // Apply text colors
        int fontColor = StyleHelper.parseColor(styleConfig.getOrDefault("tab-font-color", "#2273F7"));
        int fontColorSelected = StyleHelper.parseColor(styleConfig.getOrDefault("tab-font-color-selected", "#000000"));
        layout.setTabTextColors(fontColor, fontColorSelected);

        return layout;
    }

    /**
     * Apply indicator style
     */
    private void applyIndicatorStyle(CustomTabLayout tabLayout,
                                     Context context,
                                     ComponentStyleConfig.StyleHashMap<String, String> styleConfig) {
        // Get configuration
        String indicatorColor = styleConfig.getOrDefault("indicator-color", "#2273F7");
        String indicatorWidth = styleConfig.getOrDefault("indicator-width", "48px");
        String indicatorHeight = styleConfig.getOrDefault("indicator-height", "8px");
        String indicatorRadius = styleConfig.getOrDefault("indicator-radius", "4px");

        // Parse indicator width (supports fixed value and percentage)
        IndicatorWidth width = parseIndicatorWidth(indicatorWidth, context);

        // Parse indicator height and corner radius
        int heightPx = StyleHelper.parseDimension(indicatorHeight, context);
        int radiusPx = StyleHelper.parseDimension(indicatorRadius, context);

        // Create and set custom indicator
        CustomTabIndicator indicator = new CustomTabIndicator(
                StyleHelper.parseColor(indicatorColor),
                width,
                heightPx,
                radiusPx
        );
        tabLayout.setSelectedTabIndicator(indicator);
        tabLayout.setSelectedTabIndicatorHeight(heightPx);
    }

    /**
     * Parse indicator width (supports fixed value and percentage)
     */
    private IndicatorWidth parseIndicatorWidth(String indicatorWidth, Context context) {
        if (indicatorWidth.endsWith("%")) {
            // Percentage mode
            try {
                float percent = Float.parseFloat(indicatorWidth.replace("%", "")) / 100f;
                return new IndicatorWidth(true, percent, 0);
            } catch (NumberFormatException e) {
                if (AGenUILogger.isLoggingEnabled()) {
                    AGenUILogger.w(TAG, "Failed to parse indicator-width percent: " + indicatorWidth);
                }
                return new IndicatorWidth(true, 0.6f, 0);
            }
        } else {
            // Fixed width mode
            int widthPx = StyleHelper.parseDimension(indicatorWidth, context);
            return new IndicatorWidth(false, 0, widthPx);
        }
    }

    /**
     * Indicator width configuration class
     */
    private static class IndicatorWidth {
        boolean isPercent;      // whether it is percentage mode
        float percentValue;     // percentage value (0.0-1.0)
        int fixedWidthPx;       // fixed width (pixels)

        IndicatorWidth(boolean isPercent, float percentValue, int fixedWidthPx) {
            this.isPercent = isPercent;
            this.percentValue = percentValue;
            this.fixedWidthPx = fixedWidthPx;
        }
    }

    /**
     * Apply text color style
     */
    private void applyTextStyle(CustomTabLayout tabLayout,
                                ComponentStyleConfig.StyleHashMap<String, String> styleConfig) {
        String tabFontColor = styleConfig.getOrDefault("tab-font-color", "#2273F7");
        String tabFontColorSelected = styleConfig.getOrDefault("tab-font-color-selected", "#000000");
        tabLayout.setTabTextColors(
                StyleHelper.parseColor(tabFontColor),
                StyleHelper.parseColor(tabFontColorSelected)
        );

        // Apply font weight
        String fontWeight = styleConfig.getOrDefault("tab-font-weight", "normal");
        String fontWeightSelected = styleConfig.getOrDefault("tab-font-weight-selected", "bold");
        int normalStyle = StyleHelper.isBoldWeight(fontWeight) ? Typeface.BOLD : Typeface.NORMAL;
        int selectedStyle = StyleHelper.isBoldWeight(fontWeightSelected) ? Typeface.BOLD : Typeface.NORMAL;
        tabLayout.setTabTextTypeface(normalStyle, selectedStyle);
    }

    /**
     * Create content container
     */
    private FrameLayout createContentContainer(Context context) {
        FrameLayout container = new FrameLayout(context);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        container.setLayoutParams(params);
        return container;
    }

    /**
     * Set up tab selection listener
     */
    private void setupTabSelectionListener() {
        tabLayout.addOnTabSelectedListener(new CustomTabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(CustomTabLayout.Tab tab) {
                int position = tab.getPosition();
                showTabContent(position);
                onTabClicked(position);
            }

            @Override
            public void onTabUnselected(CustomTabLayout.Tab tab) {
            }

            @Override
            public void onTabReselected(CustomTabLayout.Tab tab) {
            }
        });
    }

    protected void onTabClicked(int index) {
    }

    /**
     * Incrementally sync tabs with the current properties config, then restore selection.
     * Reuses existing tabs where possible: updates titles in place, adds/removes only the diff.
     */
    private void parseTabs() {
        if (tabLayout == null) {
            AGenUILogger.e(TAG, "parseTabs: tabLayout is null");
            return;
        }

        Object tabsObj = properties.get("tabs");
        if (!(tabsObj instanceof List)) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "parseTabs: tabs property is not a List or is null");
            }
            return;
        }

        List<Map<String, Object>> tabs = (List<Map<String, Object>>) tabsObj;
        int oldCount = tabLayout.getTabCount();
        int newCount = tabs.size();

        for (int i = 0; i < Math.min(oldCount, newCount); i++) {
            CustomTabLayout.Tab tab = tabLayout.getTabAt(i);
            if (tab != null) {
                tab.setText(getTabTitle(tabs.get(i)));
            }
        }

        for (int i = oldCount; i < newCount; i++) {
            CustomTabLayout.Tab newTab = tabLayout.newTab();
            newTab.setText(getTabTitle(tabs.get(i)));
            tabLayout.addTab(newTab, false);
        }

        for (int i = oldCount - 1; i >= newCount; i--) {
            tabLayout.removeTabAt(i);
        }

        if (tabLayout.getTabCount() > 0) {
            selectedIndex = Math.max(0, Math.min(selectedIndex, tabLayout.getTabCount() - 1));
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "parseTabs: oldCount=" + oldCount + ", newCount=" + newCount + ", selectedIndex=" + selectedIndex);
            }
            showTabContent(selectedIndex);
        }
    }

    private String getTabTitle(Map<String, Object> tabConfig) {
        String title = (String) tabConfig.get("title");
        return title != null ? title : "Tab";
    }


    private void normalizeTabContentLayout(View view) {
        if (view == null) {
            return;
        }

        ViewGroup.LayoutParams rawParams = view.getLayoutParams();
        int width = rawParams != null ? rawParams.width : ViewGroup.LayoutParams.MATCH_PARENT;
        int height = rawParams != null ? rawParams.height : ViewGroup.LayoutParams.WRAP_CONTENT;
        if (width <= 0) {
            width = ViewGroup.LayoutParams.MATCH_PARENT;
        }
        if (height <= 0) {
            height = ViewGroup.LayoutParams.WRAP_CONTENT;
        }

        FrameLayout.LayoutParams normalizedParams;
        if (rawParams instanceof FrameLayout.LayoutParams) {
            normalizedParams = new FrameLayout.LayoutParams((FrameLayout.LayoutParams) rawParams);
        } else if (rawParams instanceof ViewGroup.MarginLayoutParams) {
            normalizedParams = new FrameLayout.LayoutParams((ViewGroup.MarginLayoutParams) rawParams);
        } else if (rawParams != null) {
            normalizedParams = new FrameLayout.LayoutParams(rawParams);
        } else {
            normalizedParams = new FrameLayout.LayoutParams(width, height);
        }

        normalizedParams.width = width;
        normalizedParams.height = height;
        normalizedParams.leftMargin = 0;
        normalizedParams.topMargin = 0;
        normalizedParams.rightMargin = 0;
        normalizedParams.bottomMargin = 0;
        normalizedParams.gravity = Gravity.TOP | Gravity.START;
        view.setTranslationX(0f);
        view.setTranslationY(0f);
        view.setLayoutParams(normalizedParams);

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "normalizeTabContentLayout: id=" + getId()
                    + ", childClass=" + view.getClass().getSimpleName()
                    + ", width=" + width + ", height=" + height);
        }
    }

    /**
     * Get the list of child component IDs from the tab configuration
     * Used by Surface to establish parent-child relationships
     */
    public List<String> getTabChildIds() {
        List<String> childIds = new ArrayList<>();
        Object tabsObj = properties.get("tabs");

        if (tabsObj instanceof List) {
            List<Map<String, Object>> tabs = (List<Map<String, Object>>) tabsObj;

            for (Map<String, Object> tab : tabs) {
                String childId = (String) tab.get("child");
                if (childId != null) {
                    childIds.add(childId);
                }
            }
        }

        return childIds;
    }

    /**
     * Recursively find the TextView in a Tab View
     */
    private TextView findTabTextView(View view) {
        if (view instanceof TextView) {
            return (TextView) view;
        }

        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View child = viewGroup.getChildAt(i);
                TextView textView = findTabTextView(child);
                if (textView != null) {
                    return textView;
                }
            }
        }

        return null;
    }

    /**
     * Show the content for the specified tab
     */
    private void showTabContent(int index) {
        if (index < 0) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "showTabContent: invalid index=" + index);
            }
            return;
        }

        if (index < tabLayout.getTabCount()) {
            CustomTabLayout.Tab tab = tabLayout.getTabAt(index);
            if (tab != null && !tab.isSelected()) {
                tab.select();
                return;
            }
        }

        contentContainer.removeAllViews();

        if (index >= 0 && index < tabContents.size()) {
            A2UIComponent content = tabContents.get(index);
            if (content != null && content.getView() != null) {
                View view = content.getView();

                // Remove view from parent if it already has one
                if (view.getParent() != null) {
                    ((ViewGroup) view.getParent()).removeView(view);
                }

                normalizeTabContentLayout(view);
                contentContainer.addView(view);
                selectedIndex = index;
            }
        }

        notifyRenderFinish(RENDER_FINISH_TYPE_TABS_INDEX_CHANGE, 0f, 0f, index);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "showTabContent: index=" + index + ", tabContents=" + tabContents.size() + ", componentId=" + getId());
        }
    }

    /**
     * Add tab content component
     */
    @Override
    public void addChild(A2UIComponent child) {
        super.addChild(child);
        if (child != null) {
            tabContents.add(child);
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "addChild: parentId=" + getId() + ", childId=" + child.getId() + ", tabContents.size=" + tabContents.size());
            }
        }
    }

    /**
     * Called when a child component's View has been created
     */
    public void onChildViewCreated(A2UIComponent child) {
        // Get expected child component count
        int expectedChildCount = 0;
        Object tabsObj = properties.get("tabs");
        if (tabsObj instanceof List) {
            expectedChildCount = ((List<?>) tabsObj).size();
        } else {
            expectedChildCount = tabContents.size();
        }

        // Check if all child component Views have been created
        int createdViewCount = 0;
        for (A2UIComponent content : tabContents) {
            if (content.getView() != null) {
                createdViewCount++;
            }
        }

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "onChildViewCreated: childId=" + (child != null ? child.getId() : "null")
                    + ", createdViews=" + createdViewCount + "/" + expectedChildCount);
        }

        if (createdViewCount == expectedChildCount && createdViewCount > 0 && contentContainer != null) {
            parseTabs();
        }
    }

    /**
     * Remove tab content component
     */
    @Override
    public void removeChild(A2UIComponent child) {
        super.removeChild(child);
        tabContents.remove(child);
    }

    /**
     * Get all tab contents
     */
    public List<A2UIComponent> getTabContents() {
        return new ArrayList<>(tabContents);
    }

    /**
     * TabsComponent manages child component Views itself
     */
    @Override
    public boolean shouldAutoAddChildView() {
        return false;
    }

    /**
     * The C++ Yoga layer writes an absolute top = kTabBarHeight onto every Tabs direct child
     * purely so updateMinHeightRecursive can sum contentH; that virtual coordinate must NOT be
     * applied to the real Android view. Returning false here mirrors the Harmony hooks
     * TabsComponent::shouldApplyChildLayout{Position,Size}, so contentContainer can lay out the
     * selected tab content at (0, 0) on its own.
     */
    @Override
    public boolean shouldApplyChildYogaLayout(A2UIComponent child) {
        return false;
    }


    @Override
    public void onUpdateProperties(Map<String, Object> changedProps) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "onUpdateProperties: id=" + getId()
                    + ", tabLayoutNull=" + (tabLayout == null)
                    + ", hasTabs=" + changedProps.containsKey("tabs")
                    + ", changedProps=" + changedProps);
        }

        if (tabLayout == null) {
            return;
        }

        if (changedProps.containsKey("tabs")) {
            parseTabs();
        }

        Map<String, Object> styles = extractStyles(changedProps);

        int tabFontColor = tabLayout.getTabNormalTextColor();
        if (styles.containsKey("tab-font-color")) {
            tabFontColor = StyleHelper.parseColor(styles.get("tab-font-color"));
        }

        int tabFontColorSelected = tabLayout.getTabSelectedTextColor();
        if (styles.containsKey("tab-font-color-selected")) {
            tabFontColorSelected = StyleHelper.parseColor(styles.get("tab-font-color-selected"));
        }

        tabLayout.setTabTextColors(tabFontColor, tabFontColorSelected);
    }

    /**
     * Custom Tab indicator
     * Supports both fixed-width and percentage-width modes, supports rounded corners, and centers automatically
     */
    private static class CustomTabIndicator extends Drawable {
        private Paint paint;
        private IndicatorWidth width;
        private int height;
        private int radius;

        CustomTabIndicator(int color, IndicatorWidth width, int height, int radius) {
            this.paint = new Paint();
            this.paint.setColor(color);
            this.paint.setAntiAlias(true);
            this.width = width;
            this.height = height;
            this.radius = radius;
        }

        @Override
        public void draw(Canvas canvas) {
            Rect bounds = getBounds();

            int indicatorWidth;
            if (width.isPercent) {
                indicatorWidth = (int) (bounds.width() * width.percentValue);
            } else {
                indicatorWidth = width.fixedWidthPx;
            }

            int margin = (bounds.width() - indicatorWidth) / 2;

            if (radius > 0) {
                RectF rectF = new RectF(
                        bounds.left + margin,
                        bounds.top,
                        bounds.right - margin,
                        bounds.bottom
                );
                canvas.drawRoundRect(rectF, radius, radius, paint);
            } else {
                canvas.drawRect(
                        bounds.left + margin,
                        bounds.top,
                        bounds.right - margin,
                        bounds.bottom,
                        paint
                );
            }
        }

        @Override
        public void setAlpha(int alpha) {
            paint.setAlpha(alpha);
        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {
            paint.setColorFilter(colorFilter);
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }

        @Override
        public int getIntrinsicHeight() {
            return height;
        }
    }
}
