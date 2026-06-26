package com.amap.agenui.render.component.factory;

import com.amap.agenui.annotation.BuiltInComponent;
import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.TextComponent;
import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.measurement.TextMeasurer;

import java.util.Map;

@BuiltInComponent
public class TextComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new TextComponent(context, id, properties);
    }

    @Override
    public String getComponentType() {
        return "Text";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return TextMeasurer::measureText;
    }
}
