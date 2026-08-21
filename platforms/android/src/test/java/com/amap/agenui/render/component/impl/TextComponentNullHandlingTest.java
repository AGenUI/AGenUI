package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.widget.TextView;

import org.junit.Test;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

/**
 * TDD tests for TextComponent null (delete-signal) handling.
 *
 * <p>Per PRD {@code agenui-null-handling-alignment.md}: when {@code text=null} arrives
 * at the leaf {@code onUpdateProperties}, the component must clear to the type-empty
 * value (empty string), NOT render the literal string "null" (which is what
 * {@code String.valueOf((Object) null)} produces today).
 *
 * <p>Seam: a Mockito mock {@link TextView} is injected via reflection into the
 * component's private {@code textView} field (and the base-class {@code view} guard),
 * because Android JVM unit tests ({@code returnDefaultValues=true}) cannot observe
 * real View state. We verify the {@link TextView#setText(CharSequence)} interaction.
 */
public class TextComponentNullHandlingTest {

    @Test
    public void onUpdateProperties_textNull_clearsToEmptyString() throws Exception {
        TextComponent c = new TextComponent((Context) null, "t1", null);
        TextView mockTv = mock(TextView.class);
        setField(c, "textView", mockTv);
        setField(c, "view", mockTv);

        c.updateProperties(map("text", null));

        verify(mockTv).setText("");
    }

    @Test
    public void onUpdateProperties_textValue_setsTextContent() throws Exception {
        TextComponent c = new TextComponent((Context) null, "t1", null);
        TextView mockTv = mock(TextView.class);
        setField(c, "textView", mockTv);
        setField(c, "view", mockTv);

        c.updateProperties(map("text", "hello"));

        verify(mockTv).setText("hello");
    }

    // ------------------------------------------------------------------
    // Reflection helpers — inject mock views without triggering createView
    // ------------------------------------------------------------------

    private static Map<String, Object> map(Object... kv) {
        Map<String, Object> m = new HashMap<>();
        for (int i = 0; i + 1 < kv.length; i += 2) {
            m.put((String) kv[i], kv[i + 1]);
        }
        return m;
    }

    private static void setField(Object target, String name, Object value) throws Exception {
        Field f = findField(target.getClass(), name);
        f.setAccessible(true);
        f.set(target, value);
    }

    private static Field findField(Class<?> cls, String name) throws NoSuchFieldException {
        for (Class<?> c = cls; c != null; c = c.getSuperclass()) {
            try {
                return c.getDeclaredField(name);
            } catch (NoSuchFieldException ignored) {
                // search up the hierarchy
            }
        }
        throw new NoSuchFieldException(name);
    }
}
