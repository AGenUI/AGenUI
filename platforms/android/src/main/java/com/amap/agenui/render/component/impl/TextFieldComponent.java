package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.Color;
import android.text.Editable;
import android.text.InputType;
import android.text.method.PasswordTransformationMethod;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.style.ComponentStyleConfig;
import com.amap.agenui.render.style.StyleHelper;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Map;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;
import com.amap.agenui.render.utils.AGenUILogger;

/**
 * TextField component implementation (compliant with A2UI v0.9 protocol, supports two-way data binding)
 *
 * Supported properties:
 * - label: label text (displayed as hint)
 * - value: text value (supports literalString or path for data binding)
 * - variant: input type (longText, number, shortText, obscured)
 *
 */
public class TextFieldComponent extends A2UIComponent {

    private static final String TAG = "TextFieldComponent";
    private static final int ERROR_COLOR = 0xFFB00020;

    private static final int ERROR_LABEL_HEIGHT_DP = 20;

    private Context context;

    private FrameLayout containerLayout;
    private EditText editText;
    private TextView errorTextView;
    private boolean isUpdatingFromNative = false;
    private boolean errorShowing = false;
    private String validationRegexp = null;
    private boolean isRegexpValidationFailing = false;
    private Pattern compiledValidationPattern = null;
    private String regexpErrorMessage = null;
    // External checks error message (separate from local regexp failure so that
    // user typing a valid value does not silently clear an external check error).
    private String externalErrorMessage = null;
    private TextWatcher textWatcher = new TextWatcher() {
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
        }

        @Override
        public void afterTextChanged(Editable s) {
            // Only send data changes to C++ when the user is typing
            if (!isUpdatingFromNative) {
                String newValue = s.toString();
                validateWithRegexp(newValue);
                sendDataChangeToNative(newValue);
            }
        }
    };

    public TextFieldComponent(Context context, String id, Map<String, Object> properties) {
        super(id, "TextField");
        this.context = context;
        if (properties != null) {
            this.properties.putAll(properties);
        }
    }

    @Override
    protected View onCreateView(Context context) {
        this.context = context;
        float density = context.getResources().getDisplayMetrics().density;
        int errorHeightPx = (int) (ERROR_LABEL_HEIGHT_DP * density);

        // Use FrameLayout to manually position editText and errorLabel within fixed bounds
        containerLayout = new FrameLayout(context) {
            @Override
            protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
                int w = right - left;
                int h = bottom - top;
                if (errorShowing && errorTextView != null) {
                    int inputH = h - errorHeightPx;
                    editText.layout(0, 0, w, inputH);
                    errorTextView.layout(0, inputH, w, h);
                } else {
                    editText.layout(0, 0, w, h);
                    if (errorTextView != null) {
                        errorTextView.layout(0, h, w, h + errorHeightPx);
                    }
                }
            }
        };
        containerLayout.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        // Create EditText (default single-line, matching iOS UITextField behavior)
        editText = new EditText(context);
        editText.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        editText.setBackground(null);
        editText.setGravity(Gravity.CENTER_VERTICAL);
        editText.setSingleLine(true);
        editText.setMaxLines(1);
        containerLayout.addView(editText);

        // Create error label (hidden by default, positioned at bottom inside bounds)
        errorTextView = new TextView(context);
        errorTextView.setTextColor(ERROR_COLOR);
        errorTextView.setTextSize(12f);
        errorTextView.setVisibility(View.GONE);
        errorTextView.setGravity(Gravity.CENTER_VERTICAL);
        FrameLayout.LayoutParams errorParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, errorHeightPx);
        errorParams.gravity = Gravity.BOTTOM;
        errorTextView.setLayoutParams(errorParams);
        containerLayout.addView(errorTextView);

        applyProperties(this.properties);

        return containerLayout;
    }

    @Override
    protected void onUpdateProperties(Map<String, Object> changedProps) {
        applyProperties(changedProps);
    }

    private void applyProperties(Map<String, Object> props) {
        if (editText == null) {
            return;
        }

        editText.removeTextChangedListener(textWatcher);

        // Update label (displayed as hint)
        if (props.containsKey("label")) {
            Object labelValue = props.get("label");
            String label = extractTextValue(labelValue);
            editText.setHint(label);
        }

        // Update text value (data update from C++)
        if (props.containsKey("value")) {
            Object textValue = props.get("value");

            // Update text content
            isUpdatingFromNative = true;
            String text = extractTextValue(textValue);
            if (!editText.getText().toString().equals(text)) {
                editText.setText(text);
                editText.setSelection(text.length());
            }
            isUpdatingFromNative = false;
        }

        // Update input type (A2UI v0.9 protocol: variant)
        if (props.containsKey("variant")) {
            String variant = String.valueOf(props.get("variant"));
            editText.setInputType(parseVariant(variant));
        }

        // Parse validationRegexp
        if (props.containsKey("validationRegexp")) {
            Object regexpObj = props.get("validationRegexp");
            validationRegexp = (regexpObj instanceof String) ? (String) regexpObj : null;
            if (validationRegexp != null && !validationRegexp.isEmpty()) {
                try {
                    compiledValidationPattern = Pattern.compile(validationRegexp);
                } catch (PatternSyntaxException e) {
                    AGenUILogger.w(TAG, "validationRegexp syntax error: " + validationRegexp, e);
                    compiledValidationPattern = null;
                }
            } else {
                compiledValidationPattern = null;
            }
        }

        // Apply styles
        if (props.containsKey("styles")) {
            Object stylesValue = props.get("styles");
            if (stylesValue instanceof Map) {
                @SuppressWarnings("unchecked")
                Map<String, Object> styles = (Map<String, Object>) stylesValue;
                applyStyles(styles);
            }
        }

        // checks adaptation - external validation error from the host. Stored
        // separately from regexp failure so display priority is: external first,
        // then regexp failure, otherwise no error.
        if (props.containsKey("checks")) {
            Object checksValue = props.get("checks");
            if (checksValue instanceof Map) {
                @SuppressWarnings("unchecked")
                Map<String, Object> checksMap = (Map<String, Object>) checksValue;
                Object resultObj = checksMap.get("result");
                boolean result = resultObj instanceof Boolean ? (Boolean) resultObj : true;
                String message = checksMap.containsKey("message") ?
                        String.valueOf(checksMap.get("message")) : "";

                if (!result && !message.isEmpty()) {
                    externalErrorMessage = message;
                } else {
                    externalErrorMessage = null;
                }
                refreshErrorDisplay();
            }
        }

        // Set text change listener (for two-way binding)
        editText.addTextChangedListener(textWatcher);
    }

    private void setError(String errorMessage) {
        if (errorMessage != null && !errorMessage.isEmpty()) {
            errorShowing = true;
            errorTextView.setText(errorMessage);
            errorTextView.setVisibility(View.VISIBLE);
            StyleHelper.setBorderColorOverride(containerLayout, ERROR_COLOR);
        } else {
            errorShowing = false;
            errorTextView.setText("");
            errorTextView.setVisibility(View.GONE);
            StyleHelper.clearBorderColorOverride(containerLayout);
        }
        containerLayout.requestLayout();
    }

    /**
     * Decide what error (if any) to show based on current external + regexp state.
     * Display priority: external checks failure > local regexp failure > no error.
     */
    private void refreshErrorDisplay() {
        String message = null;
        if (externalErrorMessage != null && !externalErrorMessage.isEmpty()) {
            message = externalErrorMessage;
        } else if (isRegexpValidationFailing) {
            message = getRegexpErrorMessage();
        }
        setError(message);
    }

    /**
     * Validate the latest text against the compiled validationRegexp pattern.
     * Empty input is treated as "not yet validated" so the placeholder state
     * does not flash an error before the user types anything.
     */
    private void validateWithRegexp(String text) {
        if (compiledValidationPattern == null || text.isEmpty()) {
            if (isRegexpValidationFailing) {
                isRegexpValidationFailing = false;
                refreshErrorDisplay();
            }
            return;
        }

        boolean matches = compiledValidationPattern.matcher(text).matches();
        boolean wasFailing = isRegexpValidationFailing;
        isRegexpValidationFailing = !matches;
        if (wasFailing != isRegexpValidationFailing) {
            refreshErrorDisplay();
        } else if (isRegexpValidationFailing) {
            // Already failing — keep error visible (covers reactivation after
            // an external check transition that may have replaced the message).
            refreshErrorDisplay();
        }
    }

    /**
     * Resolve the error message shown when validationRegexp fails. Reads from
     * the TextField theme config (`validation-error-message`) once and caches
     * it; falls back to the hard-coded "Invalid format" string otherwise.
     */
    private String getRegexpErrorMessage() {
        if (regexpErrorMessage != null) return regexpErrorMessage;
        Map<String, String> style =
                ComponentStyleConfig.getInstance(context).getComponentStyle("TextField");
        String msg = style != null ? style.get("validation-error-message") : null;
        regexpErrorMessage = msg != null ? msg : "Invalid format";
        return regexpErrorMessage;
    }

    /**
     * Extract text value (supports literalString or path)
     */
    private String extractTextValue(Object textValue) {
        if (textValue instanceof Map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> textMap = (Map<String, Object>) textValue;

            if (textMap.containsKey("literalString")) {
                return String.valueOf(textMap.get("literalString"));
            }

            if (textMap.containsKey("path")) {
                return "";
            }
        }

        return String.valueOf(textValue);
    }

    /**
     * Parse input type variant.
     */
    private int parseVariant(String variant) {
        switch (variant.toLowerCase()) {
            case "number":
                return InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL | InputType.TYPE_NUMBER_FLAG_SIGNED;
            case "longtext":
                return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
            case "obscured":
                return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
            case "shorttext":
            default:
                return InputType.TYPE_CLASS_TEXT;
        }
    }

    /**
     * Send data change to the C++ DataBinding Module
     */
    private void sendDataChangeToNative(String value) {
        try {
            JSONObject json = new JSONObject();
            json.put("value", value);
            syncState(json.toString());
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "sendDataChangeToNative: failed to build JSON", e);
        }
    }

    /**
     * Apply styles (compatible with all style properties from TextComponent).
     */
    private void applyStyles(Map<String, Object> styles) {
        if (styles == null || styles.isEmpty() || editText == null) {
            return;
        }

        editText.removeTextChangedListener(textWatcher);

        String variant = properties.containsKey("variant") ?
                String.valueOf(properties.get("variant")).toLowerCase() : "shorttext";

        // 1. Use StyleHelper for unified text style handling
        StyleHelper.applyTextStyles(editText, styles, context);

        // 2. TextField specific: handle hint color
        if (styles.containsKey("color")) {
            Object colorValue = styles.get("color");
            int color = StyleHelper.parseColor(colorValue);
            if (color != 0) {
                editText.setHintTextColor(Color.argb(128, Color.red(color), Color.green(color), Color.blue(color)));
            } else {
                editText.setHintTextColor(Color.GRAY);
            }
        }

        // 3. TextField specific: handle line-clamp and text-overflow based on variant mode
        if (variant.equals("longtext")) {
            // longText mode: support multiple lines
        } else if (variant.equals("obscured")) {
            editText.setMaxLines(1);
            editText.setSingleLine(true);
            editText.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
            editText.setTransformationMethod(PasswordTransformationMethod.getInstance());
        } else {
            editText.setMaxLines(1);
            editText.setSingleLine(true);
        }

        editText.addTextChangedListener(textWatcher);
    }
}
