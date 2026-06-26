package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.DateTimeInputComponent;
import com.amap.agenui.render.measurement.DateTimeInputMeasurer;
import com.amap.agenui.render.measurement.IMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class DateTimeInputComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new DateTimeInputComponent(id, properties);
    }

    @Override
    public String getComponentType() {
        return "DateTimeInput";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return DateTimeInputMeasurer::measure;
    }
}
