package com.amap.agenui.render.component;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import com.amap.agenui.render.style.StyleHelper;

import java.util.Map;

/**
 * A2UI layout container component abstract base class
 *
 * Responsibilities:
 * 1. Inherits all common functionality from A2UIComponent
 * 2. Provides layout-container-specific style handling (overflow and container semantics)
 * 3. Uses YogaAbsoluteLayout-managed child positioning supplied by the native layout pipeline
 * 4. Provides "child container" semantics: the root View and the container that holds child
 *    components can be different objects
 *
 * Applicable components:
 * - Row, Column, Card, List, Tabs, Modal, and other container components
 */
public abstract class A2UILayoutComponent extends A2UIComponent {

    private static final String TAG = "A2UILayoutComponent";

    public A2UILayoutComponent(String id, String componentType) {
        super(id, componentType);
    }

    @Override
    public View createView(Context context, ViewGroup parent) {
        View view = super.createView(context, parent);
        if (view != null) {
            // Recursively create any children whose creation was deferred
            // (e.g. children added while parent was lazy: shouldCreateChildView=false on parent chain).
            createChildViews(context);
        }
        if (view instanceof ViewGroup) {
            applyLayoutStyles((ViewGroup) view, properties);
        }
        return view;
    }

    /**
     * Recursively walks the children list and triggers createView on any child
     * whose view has not yet been created. Called after super.createView() so that
     * when a lazy-loaded subtree is finally activated (e.g. a ListComponent's Card
     * scrolls onto screen and gets bound), the entire subtree under it materializes
     * in one pass.
     *
     * Honors {@link #shouldAutoAddChildView()}: if the container manages child
     * placement itself (e.g. ListComponent backed by RecyclerView), we do NOT
     * call addView here — the container's adapter/holder handles that.
     */
    protected void createChildViews(Context context) {
        ViewGroup container = getChildContainer();

        for (A2UIComponent child : children) {
            if (child.isViewCreated()) continue;
            View childView = child.createView(context, container);
            if (childView == null) continue;

            if (!shouldAutoAddChildView()) {
                // Container manages its own children (Tabs/Modal/List). Notify the parent
                // hook so it can place the child view itself, mirroring
                // Surface.attachChildView's behavior.
                onChildViewCreated(child);
            } else if (container != null && childView.getParent() == null) {
                container.addView(childView);
            }
        }
    }

    @Override
    public boolean shouldCreateChildView() {
        if (parent != null && !parent.shouldCreateChildView()) {
            return false;
        }
        return true;
    }

    /**
     * Returns the container that actually holds child components.
     *
     * By default, the layout component's root View is the child container.
     * Special components (e.g. a HorizontalScrollView wrapping an inner LinearLayout)
     * can override this method.
     */
    public ViewGroup getChildContainer() {
        View root = getView();
        if (root instanceof ViewGroup) {
            return (ViewGroup) root;
        }
        return null;
    }

    @Override
    public void onUpdateProperties(Map<String, Object> changedProps) {
        if (view != null) {
            if (view instanceof ViewGroup) {
                applyLayoutStyles((ViewGroup) view, this.properties);
            }
        }
    }

    private void applyLayoutStyles(ViewGroup viewGroup, Map<String, Object> properties) {
        Map<String, Object> styles = extractStyles(properties);
        if (styles == null || styles.isEmpty()) {
            return;
        }

        StyleHelper.applyOverflow(viewGroup, styles);
    }

    @Override
    public void addChild(A2UIComponent child) {
        super.addChild(child);
    }
}
