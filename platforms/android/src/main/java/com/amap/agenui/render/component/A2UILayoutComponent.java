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
        if (view instanceof ViewGroup) {
            applyLayoutStyles((ViewGroup) view, properties);
        }
        return view;
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
    public void onUpdateProperties(Map<String, Object> properties) {
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
