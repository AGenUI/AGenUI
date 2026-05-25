package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;

import com.amap.agenui.render.component.A2UILayoutComponent;
import com.amap.agenui.render.layout.YogaAbsoluteLayout;

import java.util.Map;

/**
 * List keeps horizontal scroll capability while delegating child placement to Yoga.
 */
public class ListComponent extends A2UILayoutComponent {

    private String direction = "vertical";
    private YogaAbsoluteLayout contentContainer;
    private HorizontalScrollView horizontalScrollView;

    public ListComponent(String id, Map<String, Object> properties) {
        super(id, "List");
        if (properties != null) {
            this.properties.putAll(properties);
        }
    }

    @Override
    protected View onCreateView(Context context) {
        Object directionValue = properties.get("direction");
        direction = directionValue != null ? String.valueOf(directionValue) : "vertical";
        contentContainer = new YogaAbsoluteLayout(context);
        contentContainer.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        if ("horizontal".equals(direction)) {
            horizontalScrollView = new HorizontalScrollView(context);
            horizontalScrollView.setLayoutParams(new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT));
            horizontalScrollView.setHorizontalScrollBarEnabled(false);
            horizontalScrollView.setFillViewport(false);
            horizontalScrollView.setClipChildren(false);
            horizontalScrollView.setClipToPadding(false);
            horizontalScrollView.setOverScrollMode(View.OVER_SCROLL_NEVER);
            horizontalScrollView.addView(contentContainer, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.MATCH_PARENT));
            return horizontalScrollView;
        }

        return contentContainer;
    }

    @Override
    public ViewGroup getChildContainer() {
        return contentContainer;
    }
}
