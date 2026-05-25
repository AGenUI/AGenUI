package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.cardview.widget.CardView;

import com.amap.agenui.render.component.A2UILayoutComponent;
import com.amap.agenui.render.layout.YogaAbsoluteLayout;

import java.util.Map;

/**
 * Card keeps CardView chrome while delegating child placement to Yoga.
 */
public class CardComponent extends A2UILayoutComponent {

    private CardView cardView;
    private YogaAbsoluteLayout contentContainer;

    public CardComponent(String id, Map<String, Object> properties) {
        super(id, "Card");
        if (properties != null) {
            this.properties.putAll(properties);
        }
    }

    @Override
    protected View onCreateView(Context context) {
        cardView = new CardView(context) {
            @Override
            public void setPadding(int left, int top, int right, int bottom) {
                setContentPadding(left, top, right, bottom);
            }
        };
        cardView.setLayoutParams(new ViewGroup.MarginLayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        contentContainer = new YogaAbsoluteLayout(context);
        cardView.addView(contentContainer, new CardView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        if (!properties.isEmpty()) {
            onUpdateProperties(this.properties);
        }
        return cardView;
    }

    @Override
    public ViewGroup getChildContainer() {
        return contentContainer;
    }

    @Override
    public void onUpdateProperties(Map<String, Object> properties) {
        super.onUpdateProperties(properties);
        if (cardView == null) {
            return;
        }

        Object radius = this.properties.get("radius");
        if (radius instanceof Number) {
            cardView.setRadius(dpToPx(cardView.getContext(), ((Number) radius).floatValue()));
        } else {
            cardView.setRadius(dpToPx(cardView.getContext(), 8f));
        }
        cardView.setCardBackgroundColor(0xFFFFFFFF);
        cardView.setClickable(true);
        cardView.setFocusable(true);
    }

    private float dpToPx(Context context, float dp) {
        return dp * context.getResources().getDisplayMetrics().density;
    }
}
