package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.TableComponent;
import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.measurement.TableMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class TableComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new TableComponent(context, id, properties);
    }

    @Override
    public String getComponentType() {
        return "Table";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return TableMeasurer::measure;
    }
}
