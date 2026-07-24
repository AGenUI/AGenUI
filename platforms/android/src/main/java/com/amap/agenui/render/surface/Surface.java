package com.amap.agenui.render.surface;

import android.content.Context;
import android.graphics.Canvas;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import com.amap.a2ui_sdk.R;
import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.A2UILayoutComponent;
import com.amap.agenui.render.component.ComponentEventDispatcher;
import com.amap.agenui.render.drawable.ShadowPainter;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Surface - represents an independent UI canvas
 *
 * Responsibilities:
 * 1. Manages CRUD operations on the component tree
 * 2. Maintains componentTree (component ID → component instance mapping)
 * 3. Provides interfaces for creating, updating, and destroying components
 * 4. Supports dynamic container bind/unbind (for RecyclerView optimization)
 *
 * Design notes:
 * - A Surface can exist without a container (CREATED state)
 * - Container bind/unbind is independent of the Surface lifecycle
 * - Supports pre-rendering: components can start being created before the container is bound
 *
 */
public class Surface {

    private static final String TAG = "Surface";

    /**
     * Surface state enumeration
     */

    private final String surfaceId;

    // ---- Blank Screen Detection ----
    /** Handler used for blank-screen detection (lazily created) */
    private Handler mBlankCheckHandler;
    /** Pending blank-screen detection task (used for cancellation) */
    private Runnable mBlankCheckRunnable;

    /**
     * Original raw protocol content (the full JSON string that was parsed to create this surface)
     */
    private String rawProtocolContent;
    private ViewGroup container;  // Internally created root container; always non-null
    private final Context context;
    private final ComponentEventDispatcher componentEventDispatcher;
    private final SurfaceLayoutDispatcher surfaceLayoutDispatcher;

    private volatile boolean destroyed = false;
    private A2UIComponent rootComponent;
    private final Map<String, A2UIComponent> componentTree = new HashMap<>();

    // Animation toggle - enabled by default
    private boolean animationEnabled = true;
    /**
     * Constructor
     *
     * @param surfaceId                Unique Surface identifier
     * @param context                  Android Context
     * @param componentEventDispatcher Bridge between components and the Native layer
     */
    public Surface(
            String surfaceId,
            Context context,
            ComponentEventDispatcher componentEventDispatcher,
            SurfaceLayoutDispatcher surfaceLayoutDispatcher) {
        this.surfaceId = surfaceId;
        this.context = context;
        this.componentEventDispatcher = componentEventDispatcher;
        this.surfaceLayoutDispatcher = surfaceLayoutDispatcher;

        // If a container is provided at construction time, enter the BOUND state immediately
        this.container = new FrameLayout(context){
            @Override
            protected boolean drawChild(@NonNull Canvas canvas, View child, long drawingTime) {
                ShadowPainter.drawIfNeeded(canvas, child);
                return super.drawChild(canvas, child, drawingTime);
            }
        };
        // Marks AGenUI's outermost view. ShadowPainter uses this to stop clip-disabling here so a
        // root-node shadow never mutates the host view tree above the surface (matches iOS/Harmony).
        this.container.setTag(R.id.agenui_surface_root, Boolean.TRUE);
        this.container.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT));
        this.container.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
            if (destroyed || this.surfaceLayoutDispatcher == null) {
                return;
            }
            // Report the host container's available size back to native Yoga instead of the
            // current root view size. The root view can be an auto-sized component such as a
            // Button/Card/Text; feeding that intrinsic width back as the next root constraint
            // causes the surface to collapse on itself (for example Button padding leaves
            // maxWidth=0 for the inner Text on the next layout pass).
            int width = right - left;
            int height = bottom - top;
            this.surfaceLayoutDispatcher.reportSurfaceSize(context, width, height);
        });

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "Surface created: id=" + surfaceId);
        }
    }

    /**
     * Sets the animation toggle
     *
     * @param enabled true to enable animation, false to disable
     */
    public void setAnimationEnabled(boolean enabled) {
        this.animationEnabled = enabled;
    }

    /**
     * Returns the animation toggle state
     *
     * @return true if animation is enabled, false if disabled
     */
    public boolean isAnimationEnabled() {
        return animationEnabled;
    }

    /**
     * Sets the raw protocol content
     *
     * @param rawProtocolContent Original raw protocol content
     */
    public void setRawProtocolContent(String rawProtocolContent) {
        this.rawProtocolContent = rawProtocolContent;
    }

    /**
     * Returns the raw protocol content
     *
     * @return Original raw protocol content
     */
    public String getRawProtocolContent() {
        return rawProtocolContent;
    }

    /**
     * Adds a component
     *
     * @param parentId  Parent component ID (null for root component)
     * @param component Component instance
     */
    public void addComponent(String parentId, A2UIComponent component) {
        component.setSurfaceId(this.surfaceId);
        component.setComponentBridge(this.componentEventDispatcher);
        component.setSurfaceLayoutDispatcher(this.surfaceLayoutDispatcher);
        component.setAnimationEnabledSupplier(this::isAnimationEnabled);

        componentTree.put(component.getId(), component);

        if ("root".equals(component.getId())) {
            if (rootComponent != null && rootComponent != component) {
                AGenUILogger.w(TAG, "addComponent: rootComponent already set, ignoring component=" + component.getId());
                return;
            }
            rootComponent = component;
            attachRootView(component);
        } else {
            handleChildComponent(parentId, component);
        }
    }

    private void attachRootView(A2UIComponent component) {
        View existing = component.getView();
        if (existing != null) {
            ViewParent parent = existing.getParent();
            // Only when it is already in container do we do nothing
            if (parent != container) {
                if (parent instanceof ViewGroup) {
                    ((ViewGroup) parent).removeView(existing);
                }
                container.addView(existing); // Whether parent is null or another ViewGroup, in the end we still need to add
                AGenUILogger.e(TAG, "surface root view already has parent: " + surfaceId);
            }
            return;
        }
        View view = component.createView(context, container);
        if (view != null) {
            container.addView(view);
        } else {
            AGenUILogger.e(TAG, "createView returned null for root component!");
        }
    }

    private void handleChildComponent(String parentId, A2UIComponent component) {
        A2UIComponent parent = componentTree.get(parentId);
        if (parent == null) {
            AGenUILogger.e(TAG, "Parent component not found: " + parentId);
            return;
        }
        parent.addChild(component);

        // Gate 1 — truly lazy containers (horizontal List):
        // their adapter creates child views on demand, so skip both creation
        // and attachment here.
        // Self-managed placement containers (Tabs, Modal) still need eager
        // child view creation; only the *placement* is self-managed.
        // attachChildView routes them via onChildViewCreated instead of
        // auto-adding to the parent container.
        if (!parent.shouldAutoAddChildView() && !parent.shouldCreateChildView()) {
            return;
        }

        ViewGroup parentContainer = getComponentChildContainer(parent);
        if (parentContainer == null) {
            AGenUILogger.w(TAG, "Parent childContainer is null, child view not created yet");
            return;
        }

        View childView = component.createView(context, parentContainer);
        if (childView == null) {
            AGenUILogger.e(TAG, "createView returned null for child: " + component.getId());
            return;
        }
        attachChildView(parent, component, childView, parentContainer);
    }

    private void attachChildView(A2UIComponent parent, A2UIComponent child,
                                 View childView, ViewGroup parentContainer) {
        if (!parent.shouldAutoAddChildView()) {
            notifyParentChildViewCreated(parent, child);
            return;
        }

        int index = -1;
        Object childrenObj = parent.getProperties().get("children");
        if (childrenObj instanceof List) {
            index = calculateInsertIndex((List<?>) childrenObj, parent.getChildren(), child.getId());
        }
        parentContainer.addView(childView, index);

        if (animationEnabled) {
            float targetAlpha = childView.getAlpha();
            childView.setAlpha(0f);
            childView.animate().alpha(targetAlpha).setDuration(500).start();
        }

        ViewGroup childContainer = getComponentChildContainer(child);
        if (childContainer != null && !child.getChildren().isEmpty()) {
            createChildrenViews(child, childContainer);
        }
    }


    private int calculateInsertIndex(List<?> expectedOrder,
                                     List<A2UIComponent> existingChildren,
                                     String newChildId) {

        // Build an id → expected order index map for O(1) lookup
        Map<String, Integer> orderMap = new HashMap<>();
        for (int i = 0; i < expectedOrder.size(); i++) {
            orderMap.put(String.valueOf(expectedOrder.get(i)), i);
        }

        Integer newChildExpectedIndex = orderMap.get(newChildId);
        if (newChildExpectedIndex == null) {
            return existingChildren.size(); // Not in the list; append to end
        }

        int insertIndex = 0;
        for (A2UIComponent existing : existingChildren) {
            Integer existingIndex = orderMap.get(existing.getId());
            if (existingIndex != null && existingIndex < newChildExpectedIndex) {
                insertIndex++;
            }
        }

        return insertIndex;
    }

    /**
     * Removes a component
     *
     * @param componentId Component ID
     */
    public void removeComponent(String componentId) {
        A2UIComponent component = componentTree.get(componentId);
        if (component == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Component not found: " + componentId);
            }
            return;
        }

        if (component == rootComponent) {
            AGenUILogger.e(TAG, "removeComponent: attempted to remove root component incrementally, ignored: " + surfaceId);
            return;
        }

        A2UIComponent parent = component.getParent();
        if (parent != null) {
            parent.removeChild(component);
        }

        List<String> subtreeIds = new ArrayList<>();
        collectSubtreeIds(component, subtreeIds);
        for (String subtreeId : subtreeIds) {
            componentTree.remove(subtreeId);
        }

        component.destroy();
    }

    private void collectSubtreeIds(A2UIComponent component, List<String> subtreeIds) {
        if (component == null) {
            return;
        }

        subtreeIds.add(component.getId());
        for (A2UIComponent child : component.getChildren()) {
            collectSubtreeIds(child, subtreeIds);
        }
    }

    /**
     * Updates component properties
     *
     * @param componentId Component ID
     * @param properties  Properties Map
     */
    public void updateComponent(String componentId, Map<String, Object> properties) {
        A2UIComponent component = componentTree.get(componentId);
        if (component != null) {
            component.updateProperties(properties);
        } else {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Component not found: " + componentId);
            }
        }
    }

//    /**
//     * Updates component styles
//     *
//     * @param componentId Component ID
//     * @param styles      Styles Map
//     */
//    public void updateComponentStyle(String componentId, Map<String, Object> styles) {
//        AGenUILogger.d(TAG, "updateComponentStyle: componentId=" + componentId);
//
//        A2UIComponent component = componentTree.get(componentId);
//        if (component != null) {
//            component.updateStyle(styles);
//        } else {
//            AGenUILogger.w(TAG, "Component not found: " + componentId);
//        }
//    }

    /**
     * Returns a component
     *
     * @param componentId Component ID
     * @return Component instance, or null if not found
     */
    public A2UIComponent getComponent(String componentId) {
        return componentTree.get(componentId);
    }

    /**
     * Recursively creates Views for child components.
     * Used in addComponent to recursively create Views for all child components.
     *
     * @param parentComponent Parent component instance
     * @param parentContainer Parent container ViewGroup
     */
    private void createChildrenViews(A2UIComponent parentComponent, ViewGroup parentContainer) {
        for (A2UIComponent child : parentComponent.getChildren()) {
            if (child.getView() == null) {
                View childView = child.createView(context, parentContainer);
                if (childView != null) {
                    if (parentComponent.shouldAutoAddChildView()) {
                        parentContainer.addView(childView);
                        ViewGroup childContainer = getComponentChildContainer(child);
                        if (childContainer != null && !child.getChildren().isEmpty()) {
                            createChildrenViews(child, childContainer);
                        }
                    } else {
                        notifyParentChildViewCreated(parentComponent, child);
                    }
                } else {
                    AGenUILogger.e(TAG, "Failed to create child view: " + child.getId() + ", parentId: " + parentComponent.getId());
                }
            }
        }
    }

    /**
     * Start blank-screen detection on this Surface's component tree.
     * <p>
     * After {@code delayMs} ms, recursively traverse the component tree and count components whose view width and height are both &gt; 0
     * (lcpX). If the count is less than {@code validComponentCount}, report it via
     * {@code ComponentEventDispatcher.onSurfaceError}.
     * </p>
     *
     * @param delayMs             detection delay (ms)
     * @param validComponentCount minimum lcpX component count required to consider the screen non-blank
     */
    public void startBlankCheck(long delayMs, int validComponentCount) {
        if (validComponentCount <= 0) {
            return;
        }
        cancelBlankCheck();
        if (destroyed) {
            return;
        }
        final int minCount = validComponentCount;
        mBlankCheckRunnable = () -> {
            if (destroyed) {
                return;
            }
            int[] count = {0};
            traverseForLcpX(rootComponent, count, minCount);
            boolean isBlank = count[0] < minCount;
            if (isBlank) {
                AGenUILogger.w(TAG, "BlankCheck: componentCount=" + count[0]
                        + " < minCount=" + minCount + ", surfaceId=" + surfaceId);
            } else if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "BlankCheck: pass, componentCount=" + count[0]
                        + ", surfaceId=" + surfaceId);
            }
            componentEventDispatcher.onBlankCheckResult(surfaceId, isBlank);
        };
        if (mBlankCheckHandler == null) {
            mBlankCheckHandler = new Handler(Looper.getMainLooper());
        }
        mBlankCheckHandler.postDelayed(mBlankCheckRunnable, delayMs);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "BlankCheck scheduled: delay=" + delayMs
                    + "ms, minCount=" + minCount + ", surfaceId=" + surfaceId);
        }
    }

    /**
     * Cancel the pending blank-screen detection task. Called automatically on destroy.
     */
    public void cancelBlankCheck() {
        if (mBlankCheckHandler != null && mBlankCheckRunnable != null) {
            mBlankCheckHandler.removeCallbacks(mBlankCheckRunnable);
        }
        mBlankCheckRunnable = null;
    }

    /**
     * Recursively traverse the component tree and count components whose view width and height are both &gt; 0 (lcpX).
     * Returns early once count[0] &gt;= minCount.
     */
    private void traverseForLcpX(A2UIComponent component, int[] count, int minCount) {
        if (component == null || count[0] >= minCount) {
            return;
        }
        View view = component.getView();
        if (view != null && view.getWidth() > 0 && view.getHeight() > 0) {
            count[0]++;
        }
        for (A2UIComponent child : component.getChildren()) {
            traverseForLcpX(child, count, minCount);
        }
    }

    /**
     * Destroys the Surface, cleaning up all components and views. Idempotent: repeated calls
     * have no side effects.
     */
    public void destroy() {
        if (destroyed) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "destroy: already destroyed, surfaceId=" + surfaceId);
            }
            return;
        }
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "destroy: surfaceId=" + surfaceId);
        }
        destroyed = true;
        cancelBlankCheck();

        if (rootComponent != null) {
            rootComponent.destroy();
            rootComponent = null;
        }
        componentTree.clear();

        if (Looper.myLooper() == Looper.getMainLooper()) {
            container.removeAllViews();
        } else {
            container.post(container::removeAllViews);
        }
    }


    public String getSurfaceId() {
        return surfaceId;
    }

    /**
     * Returns a read-only view of the component tree (componentId → component instance).
     * External callers should only read, not directly modify the tree structure.
     */
    public Map<String, A2UIComponent> getComponentTree() {
        return Collections.unmodifiableMap(componentTree);
    }
    /**
     * Returns the container (internally created root view; always non-null).
     * <p>
     * Callers obtain the Surface's root view via this method and add it to their own page ViewTree.
     *
     * @return Container ViewGroup
     */
    public ViewGroup getContainer() {
        return container;
    }

    public A2UIComponent getRootComponent() {
        return rootComponent;
    }

    /**
     * Returns the current state
     *
     * @return Whether the Surface has been destroyed
     */
    public boolean isDestroyed() {
        return destroyed;
    }


    public int getComponentCount() {
        return componentTree.size();
    }

    /**
     * Notifies the parent component that a child View has been created.
     * Used for special components (e.g. TabsComponent) to execute specific logic after
     * all child Views are created.
     */
    private void notifyParentChildViewCreated(A2UIComponent parent, A2UIComponent child) {
        // Call onChildViewCreated on the parent
        parent.onChildViewCreated(child);
        // If the parent does not have an onChildViewCreated method, ignore
    }

    /**
     * Starts a surface-level layout transaction so a batch of native component updates can share
     * one final layout flush.
     */
    public void beginLayoutTransaction() {
        if (surfaceLayoutDispatcher != null) {
            surfaceLayoutDispatcher.beginTransaction();
        }
    }

    /**
     * Ends the current surface-level layout transaction and flushes batched Yoga frames once.
     */
    public void endLayoutTransaction() {
        if (surfaceLayoutDispatcher != null) {
            surfaceLayoutDispatcher.endTransaction();
        }
    }

    private ViewGroup getComponentChildContainer(A2UIComponent component) {
        if (component instanceof A2UILayoutComponent) {
            return ((A2UILayoutComponent) component).getChildContainer();
        }

        View view = component.getView();
        if (view instanceof ViewGroup) {
            return (ViewGroup) view;
        }

        return null;
    }

}
