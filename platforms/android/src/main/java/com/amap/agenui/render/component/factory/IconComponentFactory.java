package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.IconComponent;
import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.measurement.IconMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class IconComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new IconComponent(context, id, properties);
    }

    @Override
    public String getComponentType() {
        return "Icon";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return (context, paramJson, maxWidth, widthMode, maxHeight, heightMode) ->
                IconMeasurer.measure(paramJson, maxWidth, widthMode, maxHeight, heightMode);
    }
}
