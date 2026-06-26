package com.amap.agenui.render.component.factory;

import android.content.Context;

import androidx.annotation.Nullable;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.component.impl.AudioPlayerComponent;
import com.amap.agenui.render.measurement.AudioPlayerMeasurer;
import com.amap.agenui.render.measurement.IMeasurer;

import java.util.Map;
import com.amap.agenui.annotation.BuiltInComponent;

@BuiltInComponent
public class AudioPlayerComponentFactory implements IComponentFactory {

    @Override
    public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
        return new AudioPlayerComponent(id, properties);
    }

    @Override
    public String getComponentType() {
        return "AudioPlayer";
    }

    @Nullable
    @Override
    public IMeasurer getMeasurer() {
        return AudioPlayerMeasurer::measure;
    }
}
