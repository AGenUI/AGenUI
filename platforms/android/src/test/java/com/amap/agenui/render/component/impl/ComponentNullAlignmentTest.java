package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;

import org.mockito.MockedStatic;
import org.mockito.Mockito;

import com.amap.agenui.render.utils.AGenUILogger;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

/**
 * TDD tests aligning Android component null (delete-signal) behavior with iOS.
 *
 * <p>Per PRD + ios-progress.md, every component property must clear to the type-empty
 * value on null (delete signal). This covers components whose null handling was
 * previously missing or used the wrong seam.
 *
 * <p>Seam: Mockito mock views injected via reflection (Android JVM unit tests can't
 * observe real View state). We verify either a view interaction (setImageDrawable,
 * clearColorFilter) or a field value (childComponentId).
 */
public class ComponentNullAlignmentTest {

    private static MockedStatic<AGenUILogger> loggerMock;

    @BeforeClass
    public static void setUpClass() {
        // AGenUILogger has a native static initializer; mock it to avoid JNI in JVM tests.
        loggerMock = Mockito.mockStatic(AGenUILogger.class);
        loggerMock.when(AGenUILogger::isLoggingEnabled).thenReturn(false);
    }

    @AfterClass
    public static void tearDownClass() {
        if (loggerMock != null) {
            loggerMock.close();
        }
    }

    // ========================================================================
    // Button: child=null → "" (was String.valueOf(null)="null")
    // ========================================================================

    @Test
    public void button_childNull_clearsToEmpty() throws Exception {
        ButtonComponent c = new ButtonComponent((Context) null, "b1", null);
        FrameLayout mockContainer = mock(FrameLayout.class);
        setField(c, "buttonContainer", mockContainer);
        setField(c, "view", mockContainer);

        c.updateProperties(map("child", null));

        assertEquals("", getField(c, "childComponentId"));
    }

    // ========================================================================
    // Icon: name=null → clear image; color=null → clearColorFilter
    // (iOS: name→nil, color→.label/default)
    // ========================================================================

    @Test
    public void icon_nameNull_clearsImage() throws Exception {
        IconComponent c = new IconComponent((Context) null, "i1", null);
        ImageView mockIv = mock(ImageView.class);
        setField(c, "imageView", mockIv);
        setField(c, "view", mockIv);

        c.updateProperties(map("name", null));

        verify(mockIv).setImageDrawable(null);
    }

    @Test
    public void icon_colorNull_clearsColorFilter() throws Exception {
        IconComponent c = new IconComponent((Context) null, "i1", null);
        ImageView mockIv = mock(ImageView.class);
        setField(c, "imageView", mockIv);
        setField(c, "view", mockIv);

        c.updateProperties(map("color", null));

        verify(mockIv).clearColorFilter();
    }

    // ========================================================================
    // Video: url=null → stopPlayback (align iOS url→"")
    // ========================================================================

    @Test
    public void video_urlNull_stopsPlayback() throws Exception {
        VideoComponent c = new VideoComponent("v1", null);
        android.widget.VideoView mockVv = mock(android.widget.VideoView.class);
        setField(c, "videoView", mockVv);
        setField(c, "view", mockVv);
        c.updateProperties(map("url", null));
        verify(mockVv).stopPlayback();
    }

    // ========================================================================
    // Slider: label=null → setText("") (align iOS label→"")
    // ========================================================================

    @Test
    public void slider_labelNull_clearsText() throws Exception {
        SliderComponent c = new SliderComponent((Context) null, "s1", null);
        android.widget.LinearLayout mockContainer = mock(android.widget.LinearLayout.class);
        android.widget.TextView mockTv = mock(android.widget.TextView.class);
        setField(c, "containerLayout", mockContainer);
        setField(c, "labelTextView", mockTv);
        setField(c, "view", mockContainer);
        c.updateProperties(map("label", null));
        verify(mockTv).setText("");
    }

    // ========================================================================
    // Carousel: autoplaySpeed=null → 3000 (align iOS autoplaySpeed→3.0)
    // ========================================================================

    @Test
    public void carousel_autoplaySpeedNull_resetsToDefault() throws Exception {
        CarouselComponent c = new CarouselComponent((Context) null, "c1", null);
        androidx.viewpager2.widget.ViewPager2 mockVp = mock(androidx.viewpager2.widget.ViewPager2.class);
        setField(c, "viewPager", mockVp);
        setField(c, "view", mockVp);
        c.updateProperties(map("autoplaySpeed", null));
        assertEquals(Integer.valueOf(3000), getField(c, "autoplaySpeed"));
    }

    // ========================================================================
    // Carousel: draggable=null → false + setUserInputEnabled(false)
    // (align iOS draggable→false)
    // ========================================================================

    @Test
    public void carousel_draggableNull_resetsToFalse() throws Exception {
        CarouselComponent c = new CarouselComponent((Context) null, "c1", null);
        androidx.viewpager2.widget.ViewPager2 mockVp = mock(androidx.viewpager2.widget.ViewPager2.class);
        setField(c, "viewPager", mockVp);
        setField(c, "view", mockVp);
        // pre-set true so null must actually reset it
        setField(c, "draggable", true);

        c.updateProperties(map("draggable", null));

        verify(mockVp).setUserInputEnabled(false);
        assertEquals(Boolean.FALSE, getField(c, "draggable"));
    }

    // ========================================================================
    // ChoicePicker: displayStyle→"", filterable→false, options→[], value→[]
    // (align iOS .deleted). parseProperties reads merged properties; when a key is
    // absent (null delete erased by base class, or never set), the else branch clears.
    // ========================================================================

    @Test
    public void choicePicker_displayStyleNull_clearsToEmpty() throws Exception {
        ChoicePickerComponent c = new ChoicePickerComponent("cp1", null);
        c.parseProperties();  // properties empty → else clears displayStyle
        assertEquals("", getField(c, "displayStyle"));
    }

    @Test
    public void choicePicker_filterableNull_clearsToFalse() throws Exception {
        ChoicePickerComponent c = new ChoicePickerComponent("cp1", null);
        c.parseProperties();
        assertEquals(Boolean.FALSE, getField(c, "filterable"));
    }

    @Test
    public void choicePicker_optionsNull_clearsToEmpty() throws Exception {
        ChoicePickerComponent c = new ChoicePickerComponent("cp1", null);
        c.parseProperties();
        assertEquals(0, ((java.util.List<?>) getField(c, "options")).size());
    }

    @Test
    public void choicePicker_valueNull_clearsSelectedChipValues() throws Exception {
        ChoicePickerComponent c = new ChoicePickerComponent("cp1", null);
        // variant=multipleSelection + value absent (simulates null delete)
        @SuppressWarnings("unchecked")
        Map<String, Object> props = (Map<String, Object>) getField(c, "properties");
        props.put("variant", "multipleSelection");
        c.parseProperties();
        assertEquals(0, ((java.util.List<?>) getField(c, "selectedChipValues")).size());
    }

    // ------------------------------------------------------------------
    // Reflection helpers
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

    private static Object getField(Object target, String name) throws Exception {
        Field f = findField(target.getClass(), name);
        f.setAccessible(true);
        return f.get(target);
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
