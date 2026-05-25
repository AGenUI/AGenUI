package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.view.View;

import com.amap.agenui.render.component.A2UILayoutComponent;
import com.amap.agenui.render.layout.YogaAbsoluteLayout;

import java.util.Map;

/**
 * Column is now only a Yoga-backed container host on Android.
 */
public class ColumnComponent extends A2UILayoutComponent {

    private YogaAbsoluteLayout container;

    public ColumnComponent(Context context, String id, Map<String, Object> properties) {
        super(id, "Column");
        if (properties != null) {
            this.properties.putAll(properties);
        }
    }

    @Override
    protected View onCreateView(Context context) {
        container = new YogaAbsoluteLayout(context);
        if (!properties.isEmpty()) {
            onUpdateProperties(this.properties);
        }
        return container;
    }
}
