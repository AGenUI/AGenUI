package com.amap.agenui.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Marks an {@link com.amap.agenui.render.component.IComponentFactory} for automatic
 * registration. The component name is derived from the class name by stripping the
 * {@code ComponentFactory} suffix (e.g. {@code VideoComponentFactory} -> {@code "Video"}).
 *
 * <p>At compile time, the annotation processor collects all annotated factories and
 * generates {@code BuiltInComponentRegistrar}. To exclude a component from a downstream
 * build, remove its source file from sourceSets — the processor never sees it.
 */
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE)
public @interface BuiltInComponent {
}
