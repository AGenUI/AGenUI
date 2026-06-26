package com.amap.agenui.render.image;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * Unit tests for ImageLoaderError factory methods and type checking.
 * All methods are pure logic with no Android framework dependencies.
 */
public class ImageLoaderErrorTest {

    // ========================================================================
    // invalidUrl — Factory Method
    // ========================================================================

    @Test
    public void invalidUrl_createsCorrectType() {
        ImageLoaderError error = ImageLoaderError.invalidUrl("http://bad");
        assertEquals(ImageLoaderError.Type.INVALID_URL, error.type);
    }

    @Test
    public void invalidUrl_messageContainsUrl() {
        ImageLoaderError error = ImageLoaderError.invalidUrl("http://bad.com/img.png");
        assertTrue(error.getMessage().contains("http://bad.com/img.png"));
    }

    @Test
    public void invalidUrl_causeIsNull() {
        ImageLoaderError error = ImageLoaderError.invalidUrl("http://x");
        assertNull(error.cause);
    }

    @Test
    public void invalidUrl_isNotCancelled() {
        ImageLoaderError error = ImageLoaderError.invalidUrl("http://x");
        assertFalse(error.isCancelled());
    }

    // ========================================================================
    // networkError — Factory Method
    // ========================================================================

    @Test
    public void networkError_createsCorrectType() {
        ImageLoaderError error = ImageLoaderError.networkError("http://timeout.com", null);
        assertEquals(ImageLoaderError.Type.NETWORK_ERROR, error.type);
    }

    @Test
    public void networkError_messageContainsUrl() {
        ImageLoaderError error = ImageLoaderError.networkError("http://timeout.com", null);
        assertTrue(error.getMessage().contains("http://timeout.com"));
    }

    @Test
    public void networkError_preservesCause() {
        Throwable original = new RuntimeException("timeout");
        ImageLoaderError error = ImageLoaderError.networkError("http://x", original);
        assertEquals(original, error.cause);
    }

    @Test
    public void networkError_nullCauseIsAccepted() {
        ImageLoaderError error = ImageLoaderError.networkError("http://x", null);
        assertNull(error.cause);
    }

    // ========================================================================
    // invalidData — Factory Methods
    // ========================================================================

    @Test
    public void invalidData_singleArg_createsCorrectType() {
        ImageLoaderError error = ImageLoaderError.invalidData("http://corrupt.png");
        assertEquals(ImageLoaderError.Type.INVALID_DATA, error.type);
    }

    @Test
    public void invalidData_singleArg_messageContainsUrl() {
        ImageLoaderError error = ImageLoaderError.invalidData("http://corrupt.png");
        assertTrue(error.getMessage().contains("http://corrupt.png"));
    }

    @Test
    public void invalidData_withCause_preservesCause() {
        Throwable original = new IllegalStateException("decode failed");
        ImageLoaderError error = ImageLoaderError.invalidData("http://x", original);
        assertEquals(ImageLoaderError.Type.INVALID_DATA, error.type);
        assertEquals(original, error.cause);
    }

    // ========================================================================
    // cancelled — Factory Method
    // ========================================================================

    @Test
    public void cancelled_createsCorrectType() {
        ImageLoaderError error = ImageLoaderError.cancelled();
        assertEquals(ImageLoaderError.Type.CANCELLED, error.type);
    }

    @Test
    public void cancelled_isCancelledReturnsTrue() {
        ImageLoaderError error = ImageLoaderError.cancelled();
        assertTrue(error.isCancelled());
    }

    @Test
    public void cancelled_messageNotEmpty() {
        ImageLoaderError error = ImageLoaderError.cancelled();
        assertNotNull(error.getMessage());
        assertFalse(error.getMessage().isEmpty());
    }

    // ========================================================================
    // pathResourceError — Factory Methods
    // ========================================================================

    @Test
    public void pathResourceError_singleArg_createsCorrectType() {
        ImageLoaderError error = ImageLoaderError.pathResourceError("path://icon.png");
        assertEquals(ImageLoaderError.Type.PATH_RESOURCE_ERROR, error.type);
    }

    @Test
    public void pathResourceError_singleArg_messageContainsUrl() {
        ImageLoaderError error = ImageLoaderError.pathResourceError("path://icon.png");
        assertTrue(error.getMessage().contains("path://icon.png"));
    }

    @Test
    public void pathResourceError_withCause_preservesCause() {
        Throwable original = new java.io.FileNotFoundException("not found");
        ImageLoaderError error = ImageLoaderError.pathResourceError("path://missing", original);
        assertEquals(ImageLoaderError.Type.PATH_RESOURCE_ERROR, error.type);
        assertEquals(original, error.cause);
    }

    // ========================================================================
    // isCancelled — Cross-type Verification
    // ========================================================================

    @Test
    public void isCancelled_networkError_returnsFalse() {
        assertFalse(ImageLoaderError.networkError("http://x", null).isCancelled());
    }

    @Test
    public void isCancelled_invalidData_returnsFalse() {
        assertFalse(ImageLoaderError.invalidData("http://x").isCancelled());
    }

    @Test
    public void isCancelled_pathResourceError_returnsFalse() {
        assertFalse(ImageLoaderError.pathResourceError("path://x").isCancelled());
    }

    // ========================================================================
    // Constructor — Direct Usage
    // ========================================================================

    @Test
    public void constructor_twoArgs_setsTypeAndMessage() {
        ImageLoaderError error = new ImageLoaderError(ImageLoaderError.Type.DECOMPRESSION_FAILED, "oops");
        assertEquals(ImageLoaderError.Type.DECOMPRESSION_FAILED, error.type);
        assertEquals("oops", error.getMessage());
        assertNull(error.cause);
    }

    @Test
    public void constructor_threeArgs_setsTypeMessageAndCause() {
        Throwable cause = new OutOfMemoryError("no memory");
        ImageLoaderError error = new ImageLoaderError(
                ImageLoaderError.Type.DECOMPRESSION_FAILED, "decompress failed", cause);
        assertEquals(ImageLoaderError.Type.DECOMPRESSION_FAILED, error.type);
        assertEquals("decompress failed", error.getMessage());
        assertEquals(cause, error.cause);
    }
}
