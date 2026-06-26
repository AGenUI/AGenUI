# ProGuard rules for AGenUI SDK

# Keep stack trace info for crash reporting
-keepattributes SourceFile,LineNumberTable,Exceptions,Signature,InnerClasses,EnclosingMethod

# ── Entry points ──────────────────────────────────────────────────────────────
-keep public class com.amap.agenui.AGenUI { public *; }
-keep public class com.amap.agenui.render.surface.SurfaceManager { public *; }

# ── Exceptions ────────────────────────────────────────────────────────────────
-keep public class com.amap.agenui.render.surface.ThemeException { *; }

# ── Interfaces that host apps implement ───────────────────────────────────────
-keep public interface com.amap.agenui.render.surface.ISurfaceManagerListener { *; }
-keep class * implements com.amap.agenui.render.surface.ISurfaceManagerListener { *; }
-keep public class com.amap.agenui.render.surface.Surface { public *; }

-keep public interface com.amap.agenui.function.IFunction { *; }
-keep class * implements com.amap.agenui.function.IFunction { *; }

-keep public interface com.amap.agenui.render.component.IComponentFactory { *; }

-keep public interface com.amap.agenui.render.measurement.IMeasurer { *; }

-keep public interface com.amap.agenui.render.image.ImageLoader { *; }
-keep class * implements com.amap.agenui.render.image.ImageLoader { *; }

# ── Base classes that host apps extend (custom components) ────────────────────
-keep public abstract class com.amap.agenui.render.component.A2UIComponent { public protected *; }

-keep public abstract class com.amap.agenui.render.component.A2UILayoutComponent { public protected *; }

# ── Data / callback classes used in public API ────────────────────────────────
-keep public class com.amap.agenui.function.FunctionConfig { *; }
-keep public class com.amap.agenui.function.FunctionResult { *; }
-keep public interface com.amap.agenui.render.image.ImageCallback { *; }
-keep public class com.amap.agenui.render.image.ImageLoadResult { *; }
-keep public class com.amap.agenui.render.image.ImageLoaderError { *; }
-keep public class com.amap.agenui.render.image.ImageLoadOptionsKey { *; }
# SurfaceSize is constructed by host code and read by JNI (GetFloatField on width/height)
-keep public class com.amap.agenui.render.surface.SurfaceSize { *; }
# ISurfaceSizeProviderHost: SurfaceManager implements it; JNI signature references its FQN
-keep public interface com.amap.agenui.render.surface.ISurfaceSizeProviderHost { *; }

# ── JNI bridge (C++ calls back into Java via reflection) ─────────────────────
-keep public interface com.amap.agenui.IAGenUIMessageListener { *; }
-keep class * implements com.amap.agenui.IAGenUIMessageListener { *; }
# PlatformFunction is passed to native as Object; callSync/callAsync invoked via JNI
-keep class com.amap.agenui.function.PlatformFunction { *; }
# FunctionCallContext is instantiated by C++ via JNI; constructor must not be removed
-keep class com.amap.agenui.function.FunctionCallContext { *; }

# ── Logger (JNI callback from C++) ────────────────────────────────────────────
-keep class com.amap.agenui.render.utils.AGenUILogger { *; }

# ── Native methods ────────────────────────────────────────────────────────────
-keepclasseswithmembernames class * { native <methods>; }
