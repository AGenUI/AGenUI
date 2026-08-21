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
 * TDD tests for RichTextComponent null (delete-signal) handling.
 *
 * <p>Per PRD {@code agenui-null-handling-alignment.md}: when {@code text=null}
 * arrives, RichText must clear to empty content, not render the literal "null"
 * string that {@code extractTextValue(null)} (→ {@code String.valueOf(null)})
 * would otherwise feed into {@code setHtmlContent}.
 *
 * <p>Seam: Mockito mock {@link TextView} injected via reflection into the
 * private {@code textView} field (and the base-class {@code view} guard).
 */
public class RichTextComponentNullHandlingTest {

    @Test
    public void onUpdateProperties_textNull_clearsToEmptyContent() throws Exception {
        RichTextComponent c = new RichTextComponent((Context) null, "t1", null);
        TextView mockTv = mock(TextView.class);
        setField(c, "textView", mockTv);
        setField(c, "view", mockTv);

        c.updateProperties(map("text", null));

        verify(mockTv).setText("");
    }

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
