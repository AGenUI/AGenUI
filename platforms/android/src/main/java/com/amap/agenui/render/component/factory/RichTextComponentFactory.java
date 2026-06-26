package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.RichTextComponent;
import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.measurement.TextMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class RichTextComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new RichTextComponent(context, id, properties);
    }

    @Override
    public String getComponentType() {
        return "RichText";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return TextMeasurer::measureRichText;
    }
}
