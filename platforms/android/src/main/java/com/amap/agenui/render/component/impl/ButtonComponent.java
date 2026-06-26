package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import com.amap.agenui.render.component.A2UILayoutComponent;
import com.amap.agenui.render.style.ComponentStyleConfig;
import com.amap.agenui.render.style.StyleHelper;

import java.util.Map;
import com.amap.agenui.render.utils.AGenUILogger;

/**
 * Button component implementation (compliant with A2UI v0.9 protocol)
 *
 * Supported properties:
 * - child: child component ID (typically a Text or Icon component) - required
 * - variant: button style (primary, borderless)
 * - action: click action definition - required
 * - value: optional boolean value (inherited from Checkable)
 *
 * Design notes:
 * - Button is a container component that can hold one child component (Text or Icon)
 * - Uses FrameLayout as the container to support adding child components
 * - Child components are added via Surface.addComponent() and are not created inside Button
 *
 */
public class ButtonComponent extends A2UILayoutComponent {

    private static final String TAG = "ButtonComponent";

    private Context context;
    private FrameLayout buttonContainer;
    private String childComponentId;

    public ButtonComponent(Context context, String id, Map<String, Object> properties) {
        super(id, "Button");
        this.context = context;
        if (properties != null) {
            this.properties.putAll(properties);
        }
    }

    @Override
    protected View onCreateView(Context context) {
        // Use FrameLayout as the button container to support child components
        buttonContainer = new FrameLayout(context) {
            @Override
            protected void onLayout(boolean changed, int l, int t, int r, int b) {
                int pw = r - l;
                int ph = b - t;
                for (int i = 0; i < getChildCount(); i++) {
                    View child = getChildAt(i);
                    int cw = child.getMeasuredWidth();
                    int ch = child.getMeasuredHeight();
                    int cl = (pw - cw) / 2;
                    int ct = (ph - ch) / 2;
                    child.layout(cl, ct, cl + cw, ct + ch);
                }
            }
        };
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        buttonContainer.setLayoutParams(lp);

        // Note: click listener is automatically set by the base class A2UIComponent
        // No need to set it manually here

        // Important: if properties already has attributes, apply them immediately here
        if (!properties.isEmpty()) {
            applyProperties(this.properties);
        }

        return buttonContainer;
    }

    @Override
    public void onUpdateProperties(Map<String, Object> changedProps) {
        if (buttonContainer == null) {
            return;
        }
        applyProperties(changedProps);
    }

    /**
     * Apply properties to the Button.
     * @param props the property diff (only changed keys); containsKey checks
     *              naturally skip unchanged attributes.
     */
    private void applyProperties(Map<String, Object> props) {
        if (buttonContainer == null) {
            return;
        }

        // Update child component ID
        if (props.containsKey("child")) {
            childComponentId = String.valueOf(props.get("child"));
            // Note: the child component's View is automatically added via Surface.addComponent()
            // No manual handling needed here
        }

        // Note: the action property is handled by the base class A2UIComponent
        // No need to save actionDef here

        // checks adaptation
        if (props.containsKey("checks")) {
            Object checksValue = props.get("checks");
            if (checksValue instanceof Map) {
                @SuppressWarnings("unchecked")
                Map<String, Object> checksMap = (Map<String, Object>) checksValue;
                Object resultObj = checksMap.get("result");
                boolean result = resultObj instanceof Boolean ? (Boolean) resultObj : true;

                // Control clickability and enabled state
                buttonContainer.setClickable(result);
                buttonContainer.setEnabled(result);
            }
        }

        // Apply visual state: disable + styles are handled together in applyStyles
        if (props.containsKey("disable") || props.containsKey("styles")) {
            boolean isDisabled = false;
            Object disableValue = this.properties.get("disable");
            if (disableValue instanceof Boolean) {
                isDisabled = (Boolean) disableValue;
            }
            @SuppressWarnings("unchecked")
            Map<String, Object> styles = this.properties.get("styles") instanceof Map
                    ? (Map<String, Object>) this.properties.get("styles") : null;
            applyStyles(styles, isDisabled);
        }
    }

    // Note: handleClick() method has been removed; using the generic implementation from base class A2UIComponent

    /**
     * dp to px conversion
     */
    private int dpToPx(float dp) {
        return (int) (dp * context.getResources().getDisplayMetrics().density);
    }

    /**
     * Override shouldAutoAddChildView to return true.
     * Button component needs to automatically add child component Views.
     */
    @Override
    public boolean shouldAutoAddChildView() {
        return true;
    }

    /**
     * Apply styles
     *
     * @param styles     style Map
     * @param isDisabled whether in disabled state
     */
    private void applyStyles(Map<String, Object> styles, boolean isDisabled) {
        // Update clickable/enabled state
        buttonContainer.setClickable(!isDisabled);
        buttonContainer.setEnabled(!isDisabled);

        if (isDisabled) {

            // Apply disabled state opacity
            ComponentStyleConfig config = ComponentStyleConfig.getInstance(context);
            String opacityStr = config.getStyleValue("Button", "disabled-opacity", "0.5");

            try {
                float opacity = Float.parseFloat(opacityStr);
                // Ensure opacity is between 0.0 and 1.0
                opacity = Math.max(0.0f, Math.min(1.0f, opacity));
                buttonContainer.setAlpha(opacity);
            } catch (NumberFormatException e) {
                // Parse failed, use default value 0.5
                AGenUILogger.w(TAG, "Failed to parse disabled-opacity, using default 0.5");
                buttonContainer.setAlpha(0.5f);
            }
        } else {
            // Enabled state, restore full opacity
            buttonContainer.setAlpha(1.0f);

            if (styles != null && styles.containsKey("background-color")) {
                String colorStr = String.valueOf(styles.get("background-color"));
                int color = StyleHelper.parseColor(colorStr);
                if (color != 0) {
                    setButtonColor(color);
                } else {
                    setButtonColor(Color.TRANSPARENT);
                }
            } else {
                // If no background-color, set to transparent
                setButtonColor(Color.TRANSPARENT);
            }
        }
    }

    private void setButtonColor(int color) {
        Drawable drawable = buttonContainer.getBackground();
        if (drawable instanceof GradientDrawable) {
            ((GradientDrawable) drawable).setColor(color);
            buttonContainer.setBackground(drawable);
        }
    }
}
