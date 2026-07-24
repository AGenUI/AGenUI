package com.amap.agenui.render.image;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.RequestCreator;
import com.squareup.picasso.Target;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Default image loader implementation based on Picasso.
 *
 * <p>Features:
 * <ul>
 *   <li>Uses Picasso to load network images</li>
 *   <li>Picasso Targets must be held with a strong reference; otherwise they may be GC'd and callbacks lost</li>
 *   <li>Supports cancelling load tasks by requestId</li>
 * </ul>
 *
 */
class DefaultImageLoader implements ImageLoader {

    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    /**
     * requestId → Target strong-reference map to prevent Picasso Targets from being
     * garbage-collected and losing callbacks.
     */
    private final Map<String, InternalTarget> targetMap = new HashMap<>();
    private final Object lock = new Object();
    private final AtomicLong nextRequestId = new AtomicLong(1L);

    @Override
    public String loadImage(@NonNull String url, @Nullable Map<String, Object> options,
                            @NonNull ImageCallback callback) {
        String requestId = "img_req_" + nextRequestId.getAndIncrement();
        InternalTarget target = new InternalTarget(requestId, url, callback);
        synchronized (lock) {
            targetMap.put(requestId, target);
        }

        RequestCreator request = Picasso.get().load(url);

        // If options specify target dimensions, use Picasso to downsample
        if (options != null) {
            int targetWidth = extractInt(options, ImageLoadOptionsKey.WIDTH);
            int targetHeight = extractInt(options, ImageLoadOptionsKey.HEIGHT);
            if (targetWidth > 0 && targetHeight > 0) {
                request.resize(targetWidth, targetHeight).centerInside();
            } else if (targetWidth > 0) {
                request.resize(targetWidth, 0);
            } else if (targetHeight > 0) {
                request.resize(0, targetHeight);
            }
        }

        request.into(target);
        return requestId;
    }

    @Override
    public String loadImage(@NonNull String url, @NonNull ImageCallback callback) {
        return loadImage(url, null, callback);
    }

    @Override
    public void clearCache() {
    }

    /**
     * Extracts an integer value from options (compatible with Number types).
     */
    private int extractInt(@NonNull Map<String, Object> options, @NonNull String key) {
        Object value = options.get(key);
        if (value instanceof Number) {
            return ((Number) value).intValue();
        }
        return 0;
    }

    @Override
    public void cancel(@NonNull String requestId) {
        InternalTarget old;
        synchronized (lock) {
            old = targetMap.remove(requestId);
        }
        if (old != null) {
            Picasso.get().cancelRequest(old);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Internal Picasso Target implementation
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * Internal Picasso Target that bridges Picasso callbacks to ImageCallback.
     */
    private class InternalTarget implements Target {

        private final String requestId;
        private final String url;
        private final ImageCallback callback;

        InternalTarget(String requestId, String url, ImageCallback callback) {
            this.requestId = requestId;
            this.url = url;
            this.callback = callback;
        }

        @Override
        public void onBitmapLoaded(Bitmap bitmap, Picasso.LoadedFrom from) {
            synchronized (lock) {
                targetMap.remove(requestId);
            }
            boolean isFromCache = (from == Picasso.LoadedFrom.MEMORY
                    || from == Picasso.LoadedFrom.DISK);
            // Passing null for Resources uses the default screen density
            Drawable drawable = new BitmapDrawable(null, bitmap);
            ImageLoadResult result = ImageLoadResult.bitmap(drawable, isFromCache);
            mainHandler.post(() -> callback.onSuccess(result));
        }

        @Override
        public void onBitmapFailed(Exception e, Drawable errorDrawable) {
            synchronized (lock) {
                targetMap.remove(requestId);
            }
            ImageLoaderError error = ImageLoaderError.networkError(url, e);
            mainHandler.post(() -> callback.onFailure(error));
        }

        @Override
        public void onPrepareLoad(Drawable placeHolderDrawable) {
            // Callback invoked before Picasso starts loading; nothing to handle here
        }
    }
}
