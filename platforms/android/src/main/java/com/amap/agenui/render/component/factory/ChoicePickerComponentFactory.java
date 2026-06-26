package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.ChoicePickerComponent;
import com.amap.agenui.render.measurement.ChoicePickerMeasurer;
import com.amap.agenui.render.measurement.IMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class ChoicePickerComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new ChoicePickerComponent(id, properties);
    }

    @Override
    public String getComponentType() {
        return "ChoicePicker";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return ChoicePickerMeasurer::measure;
    }
}
