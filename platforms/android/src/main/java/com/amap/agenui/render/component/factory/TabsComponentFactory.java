package com.amap.agenui.render.component.factory;

import com.amap.agenui.annotation.BuiltInComponent;
import android.content.Context;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.measurement.IMeasurer;

import androidx.annotation.Nullable;
import com.amap.agenui.render.component.impl.TabsComponent;

import java.util.Map;

@BuiltInComponent
public class TabsComponentFactory implements IComponentFactory {
    
    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new TabsComponent(id, properties);
    }
    
    @Override
    public String getComponentType() {
        return "Tabs";
    }

    @Override
    @Nullable
    public IMeasurer getMeasurer() {
        return null;
    }

}