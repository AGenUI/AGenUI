package com.amap.agenui.render.component.view;

import android.content.Context;
import android.graphics.Color;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;
import java.util.Map;

import com.amap.agenui.render.style.StyleHelper;

/** Optional form title. Dimensions are in A2UI units, matching form label measurement. */
public final class FormLabelView extends TextView {
    public static final float TEXT_SIZE = 32f;
    public static final float SPACING = 16f;

    public FormLabelView(Context context) {
        super(context);
        setTextSize(TypedValue.COMPLEX_UNIT_PX, StyleHelper.standardUnitToPx(context, TEXT_SIZE));
        setTextColor(Color.BLACK);
        setIncludeFontPadding(false);
        setPadding(0, 0, 0, StyleHelper.standardUnitToPx(context, SPACING));
        setVisibility(View.GONE);
    }

    public void updateLabel(Object value) {
        if (value instanceof Map) value = ((Map<?, ?>) value).get("literalString");
        String text = value instanceof String ? (String) value : "";
        setText(text);
        setVisibility(text == null || text.isEmpty() ? View.GONE : View.VISIBLE);
    }
}
