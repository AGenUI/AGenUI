package com.amap.agenui.render.component;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.RestrictTo;

import com.amap.agenui.render.layout.YogaAbsoluteLayout;
import com.amap.agenui.render.style.StyleHelper;
import com.amap.agenui.render.surface.SurfaceLayoutDispatcher;
import com.amap.agenui.render.utils.AGenUILogger;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * A2UI component abstract base class
 *
 * Responsibilities:
 * 1. Defines basic component properties (id, type, properties)
 * 2. Manages the component lifecycle (create, update, destroy)
 * 3. Manages parent-child relationships
 * 4. Provides abstract methods for subclasses to implement specific UI logic
 *
 */
public abstract class A2UIComponent {

    private static final String TAG = "A2UIComponent";

    protected final String id;
    protected final String componentType;
    protected final Map<String, Object> properties = new HashMap<>();
    protected View view;
    protected A2UIComponent parent;
    protected final List<A2UIComponent> children = new ArrayList<>();

    /**
     * Whether this component's createView() has already executed.
     * Set to true the first time createView() runs; used by the createView guard
     * to make the call idempotent so RecyclerView re-binds do not re-create the view.
     */
    private boolean isViewCreated = false;

    /**
     * The Surface ID this component belongs to.
     * Set by Surface.addComponent().
     *
     */
    protected String surfaceId;

    /**
     * Bridge between the component and the Native layer.
     * Set by Surface.addComponent() and used to sync UI actions and data to the Native layer.
     *
     */
    private ComponentEventDispatcher componentEventDispatcher;
    private SurfaceLayoutDispatcher surfaceLayoutDispatcher;
    private ComponentState state;
    private YogaAbsoluteLayout.LayoutState appliedYogaLayout;

    /**
     * Pulls the current animation toggle from the owning Surface on demand.
     * Why: Surface.setAnimationEnabled may be called at any time after components are
     * registered; a pushed cache would go stale. Reading via supplier keeps a single
     * source of truth and is correct even inside async callbacks fired after the toggle changed.
     * Custom interface (not java.util.function.BooleanSupplier) because the latter requires API 24.
     */
    public interface BoolSupplier {
        boolean get();
    }

    private BoolSupplier animationEnabledSupplier;

    protected static final class RenderSizeReportState {
        private float lastReportedWidth = Float.NaN;
        private float lastReportedHeight = Float.NaN;

        public RenderSizeReportState() {
        }
    }

    @FunctionalInterface
    protected interface RenderSizeResolver {
        int[] resolve(View targetView, int constrainedWidthPx);
    }

    /**
     * Shared helper for components whose final size is only known after async content has rendered
     * (for example Markdown / RichText images / Lottie composition load).
     */
    protected final class AsyncRenderSizeReporter implements View.OnLayoutChangeListener {
        private final String type;
        private final String logTag;
        private final RenderSizeResolver resolver;
        private final RenderSizeReportState reportState = new RenderSizeReportState();

        private View targetView;
        private boolean enabled = true;

        private AsyncRenderSizeReporter(String type, String logTag, RenderSizeResolver resolver) {
            this.type = type;
            this.logTag = logTag;
            this.resolver = resolver;
        }

        public void bind(View targetView) {
            if (this.targetView == targetView) {
                return;
            }
            unbind();
            this.targetView = targetView;
            if (this.targetView != null) {
                this.targetView.addOnLayoutChangeListener(this);
            }
        }

        public void unbind() {
            if (this.targetView != null) {
                this.targetView.removeOnLayoutChangeListener(this);
                this.targetView = null;
            }
        }

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }

        public void request() {
            request(properties);
        }

        public void request(Map<String, Object> propertySource) {
            if (!enabled || targetView == null) {
                return;
            }
            scheduleRenderSizeReport(targetView, type, reportState, propertySource, logTag, resolver);
        }

        @Override
        public void onLayoutChange(View v,
                                   int left,
                                   int top,
                                   int right,
                                   int bottom,
                                   int oldLeft,
                                   int oldTop,
                                   int oldRight,
                                   int oldBottom) {
            request();
        }
    }

    /**
     * Constructor
     *
     * @param id            Unique component identifier
     * @param componentType Component type (Text, Button, Row, Column, etc.)
     */
    public A2UIComponent(String id, String componentType) {
        this.id = id;
        this.componentType = componentType;
    }

    /**
     * Creates the Android View.
     * Returns the existing View if it already exists, otherwise calls onCreateView to create it.
     *
     * @param context Android Context
     * @param parent  Parent container (optional)
     * @return Created View
     */
    public View createView(Context context, ViewGroup parent) {
        if (isViewCreated) {
            return view;
        }

        if (view == null) {
            view = onCreateView(context);

            if (view == null) {
                // onCreateView returned null — do NOT set isViewCreated so that
                // a subsequent call can retry when the component is ready.
                return null;
            }

            // Apply initial styles immediately after the View is created.
            // If properties have already been set (updateProperties was called before createView),
            // apply styles now.
            if (properties != null && !properties.isEmpty()) {
                applyYogaLayout(view, properties, parent, false);
                applyCommonStyles(view, properties, parent);
            }

            // Set a generic click listener (A2UI v0.9 protocol: all components support the action property).
            // If the component has an action property, set a click listener automatically.
            setupClickListener(view);
        }

        // Mark created only after view is confirmed non-null, so that a failed
        // onCreateView does not permanently block future retry attempts.
        isViewCreated = true;

        return view;
    }

    protected final AsyncRenderSizeReporter createAsyncRenderSizeReporter(String type,
                                                                          String logTag) {
        return createAsyncRenderSizeReporter(type, logTag, null);
    }

    /**
     * Creates a reusable async size reporter for components whose final size is only known after
     * the Android view has rendered real content.
     */
    protected final AsyncRenderSizeReporter createAsyncRenderSizeReporter(String type,
                                                                          String logTag,
                                                                          RenderSizeResolver resolver) {
        return new AsyncRenderSizeReporter(type, logTag, resolver);
    }

    /**
     * Updates component properties (template method).
     *
     * <p>First call: initialises {@link ComponentState} and runs the full-apply path
     * (equivalent to the initial add). Subsequent calls: uses the incremental path —
     * the diff is compared per-key against the stored values, and processing is
     * skipped entirely when nothing actually changed.
     *
     * <p>Aligned with HarmonyOS {@code A2UIComponent::updateProperties}.
     *
     * @param changedProps diff map from the core engine (only changed keys)
     */
    public final void updateProperties(Map<String, Object> changedProps) {
        if (state == null) {
            state = new ComponentState(id);
        }

        // Per-key compare against stored values; only truly changed keys are marked dirty.
        state.updateProperties(changedProps);

        // Merge diff into full properties
        this.properties.putAll(changedProps);

        // Skip the entire apply cycle when nothing actually changed.
        if (!state.isDirty()) {
            return;
        }

        if (view != null) {
            // Re-layout + common styles only when styles changed
            if (changedProps.containsKey("styles")) {
                applyYogaLayout(view, changedProps, null, true);
                applyCommonStyles(view, changedProps);
            }

            // Subclass diff hook
            onUpdateProperties(changedProps);

            // Action catch-up
            if (changedProps.containsKey("action")) {
                setupClickListener(view);
            }
        }

        state.clearDirty();
    }

    /**
     * Applies styles common to all components.
     * Called automatically by the base class; subclasses do not need to handle this.
     *
     * Styles are read from the "styles" field in properties (JSON String or Map).
     *
     * Supported styles:
     * - Display:    display, opacity
     * - Background: background-color, background
     * - Border:     border-radius, border-color, border-width
     * - Shadow:     box-shadow
     * - Filter:     filter
     *
     * @param view       The component's View
     * @param properties Properties Map
     * @param parent     Parent container (optional)
     */
    private void applyCommonStyles(View view, Map<String, Object> properties, ViewGroup parent) {
        // Get the styles object from properties
        Map<String, Object> styles = extractStyles(properties);
        if (styles == null || styles.isEmpty()) {
            return;
        }
        StyleHelper.applyDisplay(view, styles);
        StyleHelper.applyBackground(view, styles);
        StyleHelper.applyBorder(view, styles);
        StyleHelper.applyFilter(view, styles);
    }

    /**
     * Applies common styles without a parent parameter (used by updateProperties)
     */
    private void applyCommonStyles(View view, Map<String, Object> properties) {
        applyCommonStyles(view, properties, null);
    }

    /**
     * Extracts the styles object from properties.
     *
     * styles may be:
     * 1. Map<String, Object> - used directly
     * 2. String (JSON)       - parsed first
     * 3. null                - returns an empty Map
     *
     * @param properties Properties Map
     * @return Styles Map
     */
    protected Map<String, Object> extractStyles(Map<String, Object> properties) {
        Object stylesObj = properties.get("styles");
        if (stylesObj == null) {
            return new HashMap<>();
        }

        // If it's already a Map, return it directly
        if (stylesObj instanceof Map) {
            return (Map<String, Object>) stylesObj;
        }

        // If it's a JSON String, parse it into a Map
        if (stylesObj instanceof String) {
            try {
                String jsonStr = (String) stylesObj;
                // Simple JSON parsing (using org.json or another JSON library available in the project)
                return parseJsonToMap(jsonStr);
            } catch (Exception e) {
                AGenUILogger.e(TAG, "Failed to parse styles JSON: " + stylesObj, e);
                return new HashMap<>();
            }
        }

        return new HashMap<>();
    }

    /**
     * Parses a JSON string into a Map
     *
     * @param jsonStr JSON string
     * @return Map object
     */
    protected Map<String, Object> parseJsonToMap(String jsonStr) {
        try {
            JSONObject jsonObject = new JSONObject(jsonStr);
            Map<String, Object> map = new HashMap<>();

            Iterator<String> keys = jsonObject.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                Object value = jsonObject.get(key);
                map.put(key, value);
            }

            return map;
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "JSON parse error: " + jsonStr, e);
            return new HashMap<>();
        }
    }

    /**
     * Adds a child component
     *
     * @param child Child component
     */
    public void addChild(A2UIComponent child) {
        children.add(child);
        child.parent = this;
    }

    /**
     * Removes a child component
     *
     * @param child Child component instance
     */
    public void removeChild(A2UIComponent child) {
        children.remove(child);
        if (child != null) {
            child.parent = null;
            if (child.getView() != null && this.getView() instanceof ViewGroup) {
                ((ViewGroup) this.getView()).removeView(child.getView());
            }
        }
    }

    /**
     * Removes a child component by ID
     *
     * @param childId Child component ID
     */
    public void removeChildById(String childId) {
        A2UIComponent childToRemove = null;
        for (A2UIComponent child : children) {
            if (child.getId().equals(childId)) {
                childToRemove = child;
                break;
            }
        }
        if (childToRemove != null) {
            removeChild(childToRemove);
        } else {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "removeChildById: child not found, id=" + childId);
            }
        }
    }

    /**
     * Destroys the component, recursively destroying all child components.
     * Execution order: recursively destroy children → onDestroy() (subclass releases resources)
     * → remove View from parent container.
     * Subclasses must not override this method; implement onDestroy() to release their own resources.
     */
    public final void destroy() {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "destroy: " + this.getId() + " (" + this.getComponentType() + ")");
        }

        // 1. Recursively destroy child components
        for (A2UIComponent child : children) {
            child.destroy();
        }
        children.clear();

        // 2. Subclass releases its own resources (MediaPlayer, Handler, etc.)
        onDestroy();

        // 3. Remove View from parent container
        if (view != null) {
            if (view.getParent() instanceof ViewGroup) {
                ((ViewGroup) view.getParent()).removeView(view);
            }
            view = null;
        }
    }

    /**
     * Subclass resource-release hook, called by destroy() after children are cleaned up
     * and before the View is removed.
     * Subclasses override this method to release resources such as MediaPlayer and Handler.
     */
    protected void onDestroy() {
        // Default empty implementation
    }

    /**
     * Whether child component Views should be automatically added to the parent container.
     *
     * Returns true by default, meaning child Views are added automatically.
     * Special components (e.g. TabsComponent, ModalComponent) that manage their child Views
     * themselves can override this method to return false.
     *
     * @return true to auto-add child Views, false if the component manages them internally
     */
    public boolean shouldAutoAddChildView() {
        return true;
    }

    /**
     * Callback when a child View has been created (for special parent components such as
     * TabsComponent and ModalComponent to override).
     *
     * @param child The child component whose View has been created
     */
    public void onChildViewCreated(A2UIComponent child) {
        // Default empty implementation; subclasses override as needed
    }

    /**
     * Whether this component allows its Yoga-computed x/y/width/height frame to be applied to
     * a given child's real Android view.
     *
     * Containers that manage child geometry themselves (TabsComponent's contentContainer is the
     * motivating case) should override this to return false. Counterpart to Harmony's
     * shouldApplyChildLayoutPosition / shouldApplyChildLayoutSize hooks.
     *
     * @param child The child component requesting layout application
     * @return true to apply the Yoga frame as usual, false to skip
     */
    public boolean shouldApplyChildYogaLayout(A2UIComponent child) {
        return true;
    }


    /**
     * Creates the concrete Android View.
     * Subclasses implement this method to create the corresponding View
     * (TextView, Button, LinearLayout, etc.).
     *
     * @param context Android Context
     * @return Created View
     */
    protected abstract View onCreateView(Context context);

    /**
     * Handles the concrete logic for updating component properties.
     * Subclasses implement this method to process property and style updates
     * (e.g. text, color, fontSize, backgroundColor, padding, etc.).
     *
     * @param changedProps diff map containing only changed keys; use
     *                     {@code this.properties} for the full merged state
     */
    protected abstract void onUpdateProperties(Map<String, Object> changedProps);


    public String getId() {
        return id;
    }

    public String getComponentType() {
        return componentType;
    }

    public View getView() {
        return view;
    }

    public A2UIComponent getParent() {
        return parent;
    }

    public List<A2UIComponent> getChildren() {
        return Collections.unmodifiableList(children);
    }

    public Map<String, Object> getProperties() {
        return Collections.unmodifiableMap(properties);
    }

    /**
     * Whether this component should be created eagerly when its parent calls addChild,
     * or deferred until it actually becomes visible (lazy-load).
     *
     * Default: true. Propagates parent-chain false — once any ancestor returns false,
     * all descendants return false.
     *
     * Containers that want to lazy-load their subtree (such as ListComponent) override
     * this to return false. Surface.handleChildComponent reads this on the *child*
     * being added to decide whether to skip createView+addView.
     */
    public boolean shouldCreateChildView() {
        return true;
    }

    /**
     * Whether createView() has already executed on this component.
     * Used by RecyclerView re-bind paths to avoid recreating views.
     */
    public boolean isViewCreated() {
        return isViewCreated;
    }

    /**
     * Sets the Surface ID this component belongs to.
     *
     * Design notes:
     * - Called by Surface.addComponent()
     * - Set immediately after the component is created; does not change during its lifetime
     * - Used to identify the owning Surface when the component interacts with the Native engine
     *
     * @param surfaceId Unique Surface identifier
     */
    public void setSurfaceId(String surfaceId) {
        this.surfaceId = surfaceId;
    }

    /**
     * Returns the Surface ID this component belongs to
     *
     * @return Surface ID, or null if not set
     */
    public String getSurfaceId() {
        return surfaceId;
    }

    /**
     * Wires the supplier that reads the owning Surface's animation toggle on demand.
     * Called by Surface.addComponent().
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    public void setAnimationEnabledSupplier(BoolSupplier supplier) {
        this.animationEnabledSupplier = supplier;
    }

    /**
     * Current animation toggle. Defaults to true when no supplier is wired
     * (e.g. component used standalone in tests).
     */
    protected boolean isAnimationEnabled() {
        return animationEnabledSupplier == null || animationEnabledSupplier.get();
    }

    /**
     * Sets the bridge between this component and the Native layer
     *
     * @param componentEventDispatcher The bridge instance
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    public void setComponentBridge(ComponentEventDispatcher componentEventDispatcher) {
        this.componentEventDispatcher = componentEventDispatcher;
    }

    /**
     * Sets a click listener when the component has an action property defined.
     * Pure display components (Text, Image, etc.) without an action do not intercept touch events.
     *
     * @param view The component's View
     */
    private void setupClickListener(View view) {
        if (properties.containsKey("action")) {
            view.setOnClickListener(v -> handleClick());
            view.setClickable(true);
            view.setFocusable(true);
        }
    }

    /**
     * Handles the component click event.
     *
     * Common click handling logic:
     * 1. Check whether an action is defined
     * 2. Check whether surfaceId is set
     * 3. Build JSON and dispatch to the Native layer
     *
     * Subclasses can override this method to implement custom click handling.
     *
     */
    protected void handleClick() {
        triggerAction();
    }


    /**
     * Triggers the component's Action event.
     * <p>
     * Equivalent to the Action triggered by a user click.
     * Custom components can call this proactively when needed
     * (e.g. for gestures, long press, or other interactions).
     *
     */
    public final void triggerAction() {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "Component action triggered: " + id + " (type: " + componentType + ")");
        }

        if (surfaceId == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "SurfaceId is null, cannot dispatch action for component: " + id);
            }
            return;
        }
        if (componentEventDispatcher == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "ComponentEventDispatcher is null, cannot dispatch action for component: " + id);
            }
            return;
        }

        componentEventDispatcher.submitUIAction(surfaceId, id, "");
    }

    /**
     * Synchronizes a component UI state change to the Native data model.
     * <p>
     * Custom components should call this when their UI state changes
     * (e.g. text input, checkbox toggle, slider movement) to sync the change
     * to the C++ DataBinding Module.
     *
     * @param jsonData Changed content (JSON format, e.g. {"value": "hello"})
     */
    public final void syncState(String jsonData) {
        if (surfaceId == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "SurfaceId is null, cannot syncState for component: " + id);
            }
            return;
        }
        if (componentEventDispatcher == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "ComponentEventDispatcher is null, cannot syncState for component: " + id);
            }
            return;
        }
        componentEventDispatcher.submitUIDataModel(surfaceId, id, jsonData);
    }

    /**
     * Submits a display event for this component, deriving parentComponentId
     * and parentType from {@link #parent}. Strips "styles" from properties and
     * injects "id" — aligned with the iOS Component.notifyAppeared() contract.
     */
    public final void notifyAppeared() {
        if (surfaceId == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "SurfaceId is null, cannot notifyAppeared for component: " + id);
            }
            return;
        }
        if (componentEventDispatcher == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "ComponentEventDispatcher is null, cannot notifyAppeared for component: " + id);
            }
            return;
        }
        if (parent == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Parent is null, cannot notifyAppeared for component: " + id);
            }
            return;
        }

        Map<String, Object> props = new HashMap<>(properties);
        props.remove("styles");
        props.put("id", id);
        componentEventDispatcher.notifyAppearedEvent(
                surfaceId, parent.id, parent.componentType, props);
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    public void setSurfaceLayoutDispatcher(SurfaceLayoutDispatcher surfaceLayoutDispatcher) {
        this.surfaceLayoutDispatcher = surfaceLayoutDispatcher;
    }

    /**
     * Reports an async render-finish event in A2UI units.
     */
    protected final void notifyRenderFinish(String type, float width, float height) {
        notifyRenderFinish(type, width, height, -1);
    }

    /**
     * Reports an async render-finish event, optionally carrying extra component-specific payload
     * such as Tabs' selected index.
     */
    protected final void notifyRenderFinish(String type,
                                            float width,
                                            float height,
                                            int selectedIndex) {
        if (surfaceLayoutDispatcher == null) {
            return;
        }
        surfaceLayoutDispatcher.notifyRenderFinish(id, type, width, height, selectedIndex);
    }

    /**
     * Convenience wrapper that converts Android px to A2UI units before notifying native.
     */
    protected final void notifyRenderFinishFromPx(String type, float widthPx, float heightPx) {
        if (view == null) {
            return;
        }
        notifyRenderFinish(
                type,
                StyleHelper.pxToA2ui(view.getContext(), widthPx),
                StyleHelper.pxToA2ui(view.getContext(), heightPx));
    }

    /**
     * Schedules a render-size probe on the view thread after the current layout pass.
     */
    protected final void scheduleRenderSizeReport(View targetView,
                                                  String type,
                                                  RenderSizeReportState reportState,
                                                  Map<String, Object> propertySource,
                                                  String logTag) {
        scheduleRenderSizeReport(targetView, type, reportState, propertySource, logTag, null);
    }

    protected final void scheduleRenderSizeReport(View targetView,
                                                  String type,
                                                  RenderSizeReportState reportState,
                                                  Map<String, Object> propertySource,
                                                  String logTag,
                                                  RenderSizeResolver resolver) {
        if (targetView == null) {
            return;
        }
        targetView.post(() -> reportRenderSizeIfNeeded(
                targetView,
                type,
                reportState,
                propertySource,
                logTag,
                resolver));
    }

    /**
     * Measures the current rendered size and emits render-finish only when the size changed.
     */
    protected final boolean reportRenderSizeIfNeeded(View targetView,
                                                     String type,
                                                     RenderSizeReportState reportState,
                                                     Map<String, Object> propertySource,
                                                     String logTag) {
        return reportRenderSizeIfNeeded(targetView, type, reportState, propertySource, logTag, null);
    }

    protected final boolean reportRenderSizeIfNeeded(View targetView,
                                                     String type,
                                                     RenderSizeReportState reportState,
                                                     Map<String, Object> propertySource,
                                                     String logTag,
                                                     RenderSizeResolver resolver) {
        if (targetView == null || reportState == null) {
            return false;
        }

        int widthPx = resolveRenderReportWidthPx(targetView, propertySource);
        if (widthPx <= 0) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(logTag, "⏭ [RENDER_FINISH] Skip report for " + id + ", unresolved width");
            }
            return false;
        }

        int[] resolvedSize = resolver != null
                ? resolver.resolve(targetView, widthPx)
                : measureViewForRenderReport(targetView, widthPx);
        if (resolvedSize == null || resolvedSize.length < 2) {
            return false;
        }

        int resolvedWidthPx = resolvedSize[0] > 0 ? resolvedSize[0] : widthPx;
        int resolvedHeightPx = resolvedSize[1];
        if (resolvedWidthPx <= 0 || resolvedHeightPx < 0) {
            return false;
        }

        float reportedWidth = StyleHelper.pxToA2ui(targetView.getContext(), resolvedWidthPx);
        float reportedHeight = StyleHelper.pxToA2ui(targetView.getContext(), resolvedHeightPx);
        if (Float.compare(reportedWidth, reportState.lastReportedWidth) == 0
                && Float.compare(reportedHeight, reportState.lastReportedHeight) == 0) {
            return false;
        }

        reportState.lastReportedWidth = reportedWidth;
        reportState.lastReportedHeight = reportedHeight;
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(logTag, "📏 [RENDER_FINISH] " + type + " " + id
                    + " report size: width=" + reportedWidth
                    + ", height=" + reportedHeight
                    + " (px=" + resolvedWidthPx + "x" + resolvedHeightPx + ")");
        }
        notifyRenderFinish(type, reportedWidth, reportedHeight);
        return true;
    }

    /**
     * Resolves the width constraint that should be used when probing rendered content height.
     */
    protected final int resolveRenderReportWidthPx(View targetView, Map<String, Object> propertySource) {
        if (targetView == null) {
            return 0;
        }

        int widthFromStyles = resolveWidthFromStylesPx(targetView, propertySource);
        if (widthFromStyles > 0) {
            return widthFromStyles;
        }

        int currentWidth = targetView.getWidth();
        if (currentWidth > 0) {
            return currentWidth;
        }

        int measuredWidth = targetView.getMeasuredWidth();
        if (measuredWidth > 0) {
            return measuredWidth;
        }

        int parentWidth = resolveParentWidthPx(targetView);
        if (parentWidth > 0) {
            return parentWidth;
        }

        return targetView.getResources().getDisplayMetrics().widthPixels;
    }

    protected final int resolveParentWidthPx(View targetView) {
        if (targetView == null || !(targetView.getParent() instanceof View)) {
            return 0;
        }

        View parent = (View) targetView.getParent();
        int parentWidth = parent.getWidth();
        if (parentWidth > 0) {
            return parentWidth;
        }

        return parent.getMeasuredWidth();
    }

    private int resolveWidthFromStylesPx(View targetView, Map<String, Object> propertySource) {
        Map<String, Object> styles = extractStyles(propertySource != null ? propertySource : properties);
        if (styles == null || !styles.containsKey("width")) {
            return 0;
        }

        Object widthValue = styles.get("width");
        if (widthValue instanceof Number) {
            return Math.max(0, Math.round(StyleHelper.standardUnitToPx(targetView.getContext(), ((Number) widthValue).floatValue())));
        }

        int parsedWidth = StyleHelper.parseDimension(widthValue, targetView.getContext());
        if (parsedWidth > 0) {
            return parsedWidth;
        }
        if (parsedWidth == ViewGroup.LayoutParams.MATCH_PARENT) {
            return resolveParentWidthPx(targetView);
        }

        return 0;
    }

    private int[] measureViewForRenderReport(View targetView, int widthPx) {
        int widthSpec = View.MeasureSpec.makeMeasureSpec(widthPx, View.MeasureSpec.EXACTLY);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        targetView.measure(widthSpec, heightSpec);
        return new int[]{widthPx, targetView.getMeasuredHeight()};
    }

    /**
     * Applies the Yoga frame encoded in component styles to the target view.
     *
     * When a SurfaceLayoutDispatcher is present, updates are batched into the surface-level
     * transaction; otherwise they are applied directly to the parent container.
     */
    private void applyYogaLayout(View targetView,
                                 Map<String, Object> propertySource,
                                 ViewGroup explicitParent,
                                 boolean allowDispatch) {
        if (targetView == null || propertySource == null || propertySource.isEmpty()) {
            return;
        }
        if (parent != null && !parent.shouldApplyChildYogaLayout(this)) {
            return;
        }
        Map<String, Object> styles = extractStyles(propertySource);
        YogaAbsoluteLayout.LayoutState nextLayout = buildYogaLayout(styles, targetView);
        if (nextLayout == null || nextLayout.equals(appliedYogaLayout)) {
            return;
        }

        ViewGroup parent = explicitParent;
        if (parent == null && targetView.getParent() instanceof ViewGroup) {
            parent = (ViewGroup) targetView.getParent();
        }
        if (parent == null) {
            return;
        }

        if (surfaceLayoutDispatcher != null && allowDispatch) {
            prepareYogaLayoutParamsForDispatch(targetView, parent, nextLayout);
            surfaceLayoutDispatcher.dispatchLayout(
                    targetView,
                    parent,
                    nextLayout,
                    shouldMeasureWrapContentHeightWhenYogaHeightIsZero());
        } else {
            applyYogaLayoutDirectly(targetView, parent, nextLayout);
        }
        appliedYogaLayout = nextLayout;
    }

    /**
     * Converts `styles.x/y/width/height/z-index` from A2UI units into an immutable px frame.
     */
    private YogaAbsoluteLayout.LayoutState buildYogaLayout(Map<String, Object> styles, View targetView) {
        if (styles == null || !hasYogaFrame(styles)) {
            return null;
        }
        int xPx = Math.round(StyleHelper.standardUnitToPx(targetView.getContext(), readA2uiFloat(styles.get("x"))));
        int yPx = Math.round(StyleHelper.standardUnitToPx(targetView.getContext(), readA2uiFloat(styles.get("y"))));
        int widthPx = Math.max(0, Math.round(StyleHelper.standardUnitToPx(targetView.getContext(), readA2uiFloat(styles.get("width")))));
        int heightPx = Math.max(0, Math.round(StyleHelper.standardUnitToPx(targetView.getContext(), readA2uiFloat(styles.get("height")))));
        int zIndex = readInt(styles.get("z-index"), readInt(styles.get("zIndex"), 0));
        return new YogaAbsoluteLayout.LayoutState(xPx, yPx, widthPx, heightPx, zIndex);
    }

    /**
     * Applies a Yoga frame immediately when no surface transaction dispatcher is available.
     */
    private void applyYogaLayoutDirectly(View targetView,
                                         ViewGroup parent,
                                         YogaAbsoluteLayout.LayoutState layoutState) {
        if (parent instanceof YogaAbsoluteLayout) {
            YogaAbsoluteLayout.YogaLayoutParams params = targetView.getLayoutParams() instanceof YogaAbsoluteLayout.YogaLayoutParams
                    ? (YogaAbsoluteLayout.YogaLayoutParams) targetView.getLayoutParams()
                    : new YogaAbsoluteLayout.YogaLayoutParams(layoutState.widthPx, layoutState.heightPx);
            params.yogaX = layoutState.xPx;
            params.yogaY = layoutState.yPx;
            params.yogaWidth = layoutState.widthPx;
            params.yogaHeight = layoutState.heightPx;
            params.width = layoutState.widthPx;
            params.height = layoutState.heightPx;
            params.zIndex = layoutState.zIndex;
            params.measureWrapContentHeightWhenZero = shouldMeasureWrapContentHeightWhenYogaHeightIsZero();
            targetView.setLayoutParams(params);
            return;
        }

        ViewGroup.LayoutParams rawParams = targetView.getLayoutParams();
        ViewGroup.MarginLayoutParams params;
        if (rawParams instanceof ViewGroup.MarginLayoutParams) {
            params = (ViewGroup.MarginLayoutParams) rawParams;
        } else if (rawParams != null) {
            params = new ViewGroup.MarginLayoutParams(rawParams);
        } else {
            params = new ViewGroup.MarginLayoutParams(layoutState.widthPx, layoutState.heightPx);
        }
        params.width = layoutState.widthPx;
        params.height = layoutState.heightPx;
        params.leftMargin = layoutState.xPx;
        params.topMargin = layoutState.yPx;
        targetView.setLayoutParams(params);
        targetView.setZ(layoutState.zIndex);
    }

    /**
     * The batched dispatcher path only carries Yoga frame numbers, so component-specific measure
     * hints must be attached to LayoutParams before the frame is flushed into YogaAbsoluteLayout.
     */
    private void prepareYogaLayoutParamsForDispatch(View targetView,
                                                    ViewGroup parent,
                                                    YogaAbsoluteLayout.LayoutState layoutState) {
        if (!(parent instanceof YogaAbsoluteLayout)) {
            return;
        }
        YogaAbsoluteLayout.YogaLayoutParams params = targetView.getLayoutParams() instanceof YogaAbsoluteLayout.YogaLayoutParams
                ? (YogaAbsoluteLayout.YogaLayoutParams) targetView.getLayoutParams()
                : new YogaAbsoluteLayout.YogaLayoutParams(layoutState.widthPx, layoutState.heightPx);
        params.measureWrapContentHeightWhenZero = shouldMeasureWrapContentHeightWhenYogaHeightIsZero();
        targetView.setLayoutParams(params);
    }

    /**
     * Async self-sizing components can opt in so Yoga containers temporarily measure them with
     * wrap-content height when native first reports height=0.
     */
    protected boolean shouldMeasureWrapContentHeightWhenYogaHeightIsZero() {
        return false;
    }

    private boolean hasYogaFrame(Map<String, Object> styles) {
        return styles.containsKey("x")
                && styles.containsKey("y")
                && styles.containsKey("width")
                && styles.containsKey("height");
    }

    private float readA2uiFloat(Object value) {
        if (value instanceof Number) {
            return ((Number) value).floatValue();
        }
        if (value == null) {
            return 0f;
        }
        String raw = String.valueOf(value).trim().toLowerCase();
        if (raw.endsWith("px")) {
            raw = raw.substring(0, raw.length() - 2);
        }
        try {
            return Float.parseFloat(raw);
        } catch (NumberFormatException ignored) {
            return 0f;
        }
    }

    private int readInt(Object value, int defaultValue) {
        if (value instanceof Number) {
            return ((Number) value).intValue();
        }
        if (value == null) {
            return defaultValue;
        }
        try {
            return Integer.parseInt(String.valueOf(value));
        } catch (NumberFormatException ignored) {
            return defaultValue;
        }
    }
}
