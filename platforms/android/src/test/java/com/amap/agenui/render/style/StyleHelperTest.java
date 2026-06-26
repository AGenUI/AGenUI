package com.amap.agenui.render.style;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * Unit tests for StyleHelper pure-logic static methods.
 *
 * Only methods that have NO Android Context / JNI dependencies are tested here:
 * - isBoldWeight(String)
 * - extractUrlsFromCss(String)
 */
public class StyleHelperTest {

    // ========================================================================
    // isBoldWeight — Normal Path
    // ========================================================================

    @Test
    public void isBoldWeight_boldKeyword_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("bold"));
    }

    @Test
    public void isBoldWeight_boldUpperCase_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("BOLD"));
    }

    @Test
    public void isBoldWeight_boldMixedCase_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("Bold"));
    }

    @Test
    public void isBoldWeight_normalKeyword_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("normal"));
    }

    @Test
    public void isBoldWeight_normalUpperCase_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("NORMAL"));
    }

    @Test
    public void isBoldWeight_weight700_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("700"));
    }

    @Test
    public void isBoldWeight_weight500_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("500"));
    }

    @Test
    public void isBoldWeight_weight600_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("600"));
    }

    @Test
    public void isBoldWeight_weight900_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("900"));
    }

    // ========================================================================
    // isBoldWeight — Boundary Values
    // ========================================================================

    @Test
    public void isBoldWeight_weight499_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("499"));
    }

    @Test
    public void isBoldWeight_weight400_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("400"));
    }

    @Test
    public void isBoldWeight_weight100_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("100"));
    }

    @Test
    public void isBoldWeight_exactlyAt500Boundary_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("500.0"));
    }

    @Test
    public void isBoldWeight_justBelow500_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("499.9"));
    }

    // ========================================================================
    // isBoldWeight — Error Path
    // ========================================================================

    @Test
    public void isBoldWeight_nullInput_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight(null));
    }

    @Test
    public void isBoldWeight_emptyString_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight(""));
    }

    @Test
    public void isBoldWeight_whitespaceOnly_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("   "));
    }

    @Test
    public void isBoldWeight_invalidText_returnsFalse() {
        assertFalse(StyleHelper.isBoldWeight("abc"));
    }

    @Test
    public void isBoldWeight_withLeadingTrailingSpaces_bold_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("  bold  "));
    }

    @Test
    public void isBoldWeight_withLeadingTrailingSpaces_700_returnsTrue() {
        assertTrue(StyleHelper.isBoldWeight("  700  "));
    }

    // ========================================================================
    // extractUrlsFromCss — Normal Path
    // ========================================================================

    @Test
    public void extractUrlsFromCss_doubleQuotedUrl_extractsCorrectly() {
        String result = StyleHelper.extractUrlsFromCss("url(\"http://example.com/img.png\")");
        assertEquals("http://example.com/img.png", result);
    }

    @Test
    public void extractUrlsFromCss_singleQuotedUrl_extractsCorrectly() {
        String result = StyleHelper.extractUrlsFromCss("url('http://example.com/img.png')");
        assertEquals("http://example.com/img.png", result);
    }

    @Test
    public void extractUrlsFromCss_unquotedUrl_extractsCorrectly() {
        String result = StyleHelper.extractUrlsFromCss("url(http://example.com/img.png)");
        assertEquals("http://example.com/img.png", result);
    }

    @Test
    public void extractUrlsFromCss_resProtocol_extractsCorrectly() {
        String result = StyleHelper.extractUrlsFromCss("url('res://icon_close')");
        assertEquals("res://icon_close", result);
    }

    @Test
    public void extractUrlsFromCss_multipleUrls_returnsFirst() {
        String text = "background: url('first.png'), url('second.png')";
        String result = StyleHelper.extractUrlsFromCss(text);
        assertEquals("first.png", result);
    }

    @Test
    public void extractUrlsFromCss_embeddedInLargerCss_extractsCorrectly() {
        String text = "background-image: url(\"https://cdn.example.com/bg.jpg\"); color: red;";
        String result = StyleHelper.extractUrlsFromCss(text);
        assertEquals("https://cdn.example.com/bg.jpg", result);
    }

    // ========================================================================
    // extractUrlsFromCss — Boundary Values
    // ========================================================================

    @Test
    public void extractUrlsFromCss_emptyUrlContent_returnsNull() {
        String result = StyleHelper.extractUrlsFromCss("url('')");
        assertNull(result);
    }

    @Test
    public void extractUrlsFromCss_urlWithSpaces_trimmed() {
        String result = StyleHelper.extractUrlsFromCss("url(  http://example.com/img.png  )");
        assertEquals("http://example.com/img.png", result);
    }

    // ========================================================================
    // extractUrlsFromCss — Error Path
    // ========================================================================

    @Test
    public void extractUrlsFromCss_nullInput_returnsNull() {
        assertNull(StyleHelper.extractUrlsFromCss(null));
    }

    @Test
    public void extractUrlsFromCss_emptyString_returnsNull() {
        assertNull(StyleHelper.extractUrlsFromCss(""));
    }

    @Test
    public void extractUrlsFromCss_noUrlFunction_returnsNull() {
        assertNull(StyleHelper.extractUrlsFromCss("background-color: red"));
    }

    @Test
    public void extractUrlsFromCss_malformedNoClosingParen_returnsNull() {
        assertNull(StyleHelper.extractUrlsFromCss("url(http://example.com"));
    }

    @Test
    public void extractUrlsFromCss_justTheWordUrl_returnsNull() {
        assertNull(StyleHelper.extractUrlsFromCss("url"));
    }
}
