package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.Html;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.style.StyleHelper;
import com.squareup.picasso.Picasso;
import com.squareup.picasso.Target;

import java.lang.ref.WeakReference;
import java.util.Map;
import com.amap.agenui.render.utils.AGenUILogger;

/**
 * RichText component implementation - based on Android native Html.fromHtml() + Picasso ImageGetter
 * <p>
 * Supported properties:
 * - text:        HTML-formatted rich text content (required)
 * - variant:     Text style preset (h1, h2, h3, h4, h5, caption, body) (optional)
 * - linksEnable: Whether link clicks are enabled (default true)
 * - styles:      Style dictionary (text-align, color, etc.)
 * <p>
 * Supported HTML tags:
 * - Text style:  <b>, <strong>, <i>, <em>, <u>, <strike>, <del>
 * - Text color:  <font color="#xxx">
 * - Links:       <a href="...">
 * - Images:      <img src="..."> (supports network images loaded via Picasso)
 * - Paragraphs:  <p>, <br>, <div>
 * - Lists:       <ul>, <ol>, <li>
 * - Headings:    <h1> ~ <h6>
 *
 */
public class RichTextComponent extends A2UIComponent {

    private static final String TAG = "RichTextComponent";

    private Context context;
    private TextView textView;
    private boolean currentHtmlContainsImage;
    private final AsyncRenderSizeReporter asyncRenderSizeReporter =
            createAsyncRenderSizeReporter("RichText", TAG);

    public RichTextComponent(Context context, String id, Map<String, Object> properties) {
        super(id, "RichText");
        this.context = context;
        if (properties != null) {
            this.properties.putAll(properties);
        }
    }

    @Override
    protected View onCreateView(Context context) {
        textView = new TextView(context);

        // Set LayoutParams to ensure content is displayed correctly
        textView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        ));

        // Set default text size and color
        textView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        textView.setTextColor(Color.BLACK);
        asyncRenderSizeReporter.bind(textView);
        asyncRenderSizeReporter.setEnabled(false);

        // Apply initial properties
        onUpdateProperties(this.properties);

        return textView;
    }

    @Override
    protected void onUpdateProperties(Map<String, Object> properties) {
        if (textView == null) {
            return;
        }

        // Handle text-align and color from styles
        if (properties.containsKey("styles")) {
            Object stylesValue = properties.get("styles");
            if (stylesValue instanceof Map) {
                @SuppressWarnings("unchecked")
                Map<String, Object> styles = (Map<String, Object>) stylesValue;
                
                // Handle text-align
                if (styles.containsKey("text-align")) {
                    String textAlign = String.valueOf(styles.get("text-align")).toLowerCase().trim();
                    int gravity = parseTextAlign(textAlign);
                    if (gravity != -1) {
                        textView.setGravity(gravity);
                    }
                }
                
                // Handle color
                if (styles.containsKey("color")) {
                    Object colorValue = styles.get("color");
                    int color = StyleHelper.parseColor(colorValue);
                    if (color != 0) {
                        textView.setTextColor(color);
                    } else {
                        textView.setTextColor(Color.BLACK);
                    }
                }

                // filter: drop-shadow is handled by the base class via
                // StyleHelper.applyFilter (component-level shadow using elevation).
            }
        }

        // Handle link click setting (must be set before setting content)
        boolean linksEnable = true;
        if (properties.containsKey("linksEnable")) {
            linksEnable = Boolean.parseBoolean(String.valueOf(properties.get("linksEnable")));
        }

        if (linksEnable) {
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            textView.setMovementMethod(null);
        }

        // Handle HTML content update (placed last to ensure styles are applied first)
        if (properties.containsKey("text")) {
            Object textValue = properties.get("text");
            String htmlContent = extractTextValue(textValue);

            if (htmlContent != null && !htmlContent.isEmpty()) {
                setHtmlContent(htmlContent);
                if (AGenUILogger.isLoggingEnabled()) {
                    AGenUILogger.d(TAG, "📝 [CONTENT_SET] RichText " + getId() +
                            " content set, length: " + htmlContent.length());
                }
            }
        }
    }

    /**
     * Set HTML content
     */
    private void setHtmlContent(String htmlContent) {
        if (textView == null || htmlContent == null) {
            return;
        }
        currentHtmlContainsImage = htmlContent.toLowerCase().contains("<img");
        asyncRenderSizeReporter.setEnabled(currentHtmlContainsImage);

        // Use custom ImageGetter to load images
        PicassoImageGetter imageGetter = new PicassoImageGetter(textView, context, this::reportRichTextRenderSizeIfNeeded);

        // Parse HTML
        Spanned spanned;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            spanned = Html.fromHtml(htmlContent, Html.FROM_HTML_MODE_LEGACY, imageGetter, null);
        } else {
            spanned = Html.fromHtml(htmlContent, imageGetter, null);
        }

        textView.setText(spanned);
        if (currentHtmlContainsImage) {
            asyncRenderSizeReporter.request();
        }
    }

    /**
     * Extract text value (supports literalString or path)
     */
    private String extractTextValue(Object textValue) {
        if (textValue instanceof Map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> textMap = (Map<String, Object>) textValue;

            // Support literalString
            if (textMap.containsKey("literalString")) {
                return String.valueOf(textMap.get("literalString"));
            }

            // Support path (data binding)
            if (textMap.containsKey("path")) {
                return String.valueOf(textMap.get("path"));
            }
        }

        // Direct string
        return String.valueOf(textValue);
    }

    /**
     * Parse text-align value to Gravity
     * Supports: left, center, right, start, end
     * @param textAlign Alignment string
     * @return Gravity value, or -1 if parsing fails
     */
    private int parseTextAlign(String textAlign) {
        if (textAlign == null || textAlign.isEmpty()) {
            return -1;
        }

        String[] parts = textAlign.toLowerCase().trim().split("\\s+");
        int horizontal = Gravity.START;

        // Parse horizontal alignment
        String h = parts[0];
        if (h.equals("left") || h.equals("start")) {
            horizontal = Gravity.START;
        } else if (h.equals("center")) {
            horizontal = Gravity.CENTER_HORIZONTAL;
        } else if (h.equals("right") || h.equals("end")) {
            horizontal = Gravity.END;
        } else {
            return -1;  // Invalid value
        }

        // Combine with default vertical center
        return horizontal | Gravity.CENTER_VERTICAL;
    }

    /**
     * Set HTML content (public method, callable externally)
     *
     * @param htmlContent HTML-formatted content
     */
    public void setContent(String htmlContent) {
        if (htmlContent == null) {
            htmlContent = "";
        }

        final String finalContent = htmlContent;

        if (textView != null) {
            // Update UI on the main thread
            textView.post(new Runnable() {
                @Override
                public void run() {
                    setHtmlContent(finalContent);
                    if (AGenUILogger.isLoggingEnabled()) {
                        AGenUILogger.d(TAG, "📝 [CONTENT_SET_API] RichText " + getId() +
                                " content set, length: " + finalContent.length());
                    }
                }
            });
        }
    }

    /**
     * Get current content
     *
     * @return Current text content
     */
    public String getCurrentContent() {
        if (textView != null) {
            return textView.getText().toString();
        }
        return "";
    }

    @Override
    protected void onDestroy() {
        asyncRenderSizeReporter.unbind();
        textView = null;
    }

    private void reportRichTextRenderSizeIfNeeded() {
        if (textView == null || !currentHtmlContainsImage) {
            return;
        }
        asyncRenderSizeReporter.request();
    }

    /**
     * Custom ImageGetter - loads network images using Picasso
     */
    private static class PicassoImageGetter implements Html.ImageGetter {

        private final WeakReference<TextView> textViewRef;
        private final Context context;
        private final Runnable onImageLoaded;

        public PicassoImageGetter(TextView textView, Context context, Runnable onImageLoaded) {
            this.textViewRef = new WeakReference<>(textView);
            this.context = context;
            this.onImageLoaded = onImageLoaded;
        }

        @Override
        public Drawable getDrawable(String source) {
            // Create a placeholder Drawable
            final UrlDrawable urlDrawable = new UrlDrawable();

            // Load image asynchronously using Picasso
            Picasso.get()
                    .load(source)
                    .into(new ImageTarget(urlDrawable, textViewRef, onImageLoaded));

            return urlDrawable;
        }
    }

    /**
     * Placeholder Drawable - used for asynchronous image loading
     */
    private static class UrlDrawable extends BitmapDrawable {
        private Drawable drawable;

        @Override
        public void draw(Canvas canvas) {
            if (drawable != null) {
                drawable.draw(canvas);
            }
        }

        public void setDrawable(Drawable drawable) {
            this.drawable = drawable;
            if (drawable != null) {
                int width = drawable.getIntrinsicWidth();
                int height = drawable.getIntrinsicHeight();
                drawable.setBounds(0, 0, width, height);
                setBounds(0, 0, width, height);
            }
        }
    }

    /**
     * Picasso Target - handles image load completion
     */
    private static class ImageTarget implements Target {

        private final UrlDrawable urlDrawable;
        private final WeakReference<TextView> textViewRef;
        private final Runnable onImageLoaded;

        public ImageTarget(UrlDrawable urlDrawable,
                           WeakReference<TextView> textViewRef,
                           Runnable onImageLoaded) {
            this.urlDrawable = urlDrawable;
            this.textViewRef = textViewRef;
            this.onImageLoaded = onImageLoaded;
        }

        @Override
        public void onBitmapLoaded(Bitmap bitmap, Picasso.LoadedFrom from) {
            TextView textView = textViewRef.get();
            if (textView == null) {
                return;
            }

            // Create BitmapDrawable
            BitmapDrawable drawable = new BitmapDrawable(textView.getResources(), bitmap);

            // Set image dimensions (limit maximum width to TextView width)
            int maxWidth = textView.getWidth();
            if (maxWidth == 0) {
                maxWidth = textView.getResources().getDisplayMetrics().widthPixels;
            }

            int width = bitmap.getWidth();
            int height = bitmap.getHeight();

            if (width > maxWidth) {
                float ratio = (float) maxWidth / width;
                width = maxWidth;
                height = (int) (height * ratio);
            }

            drawable.setBounds(0, 0, width, height);

            // Update UrlDrawable
            urlDrawable.setDrawable(drawable);

            // Refresh TextView
            textView.setText(textView.getText());
            textView.invalidate();
            if (onImageLoaded != null) {
                onImageLoaded.run();
            }
        }

        @Override
        public void onBitmapFailed(Exception e, Drawable errorDrawable) {
            AGenUILogger.e(TAG, "Image load failed: " + e.getMessage());
        }

        @Override
        public void onPrepareLoad(Drawable placeHolderDrawable) {
            // Placeholder image can be set here
        }
    }
}
