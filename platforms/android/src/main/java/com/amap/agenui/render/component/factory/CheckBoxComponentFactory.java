package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.CheckBoxComponent;
import com.amap.agenui.render.measurement.CheckBoxMeasurer;
import com.amap.agenui.render.measurement.IMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class CheckBoxComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new CheckBoxComponent(context, id, properties);
    }

    @Override
    public String getComponentType() {
        return "CheckBox";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return CheckBoxMeasurer::measure;
    }
}
