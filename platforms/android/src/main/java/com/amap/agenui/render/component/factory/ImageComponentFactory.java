package com.amap.agenui.render.component.factory;

import com.amap.agenui.annotation.BuiltInComponent;
import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.ImageComponent;
import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.measurement.ImageMeasurer;

import java.util.Map;

@BuiltInComponent
public class ImageComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new ImageComponent(context, id, properties);
    }

    @Override
    public String getComponentType() {
        return "Image";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return (context, paramJson, maxWidth, widthMode, maxHeight, heightMode) ->
                ImageMeasurer.measure(paramJson, maxWidth, widthMode, maxHeight, heightMode);
    }
}