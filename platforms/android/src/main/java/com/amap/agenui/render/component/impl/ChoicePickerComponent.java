package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.view.CustomCheckBoxView;
import com.amap.agenui.render.style.ComponentStyleConfig;
import com.amap.agenui.render.style.StyleHelper;
import com.amap.agenui.render.utils.AGenUILogger;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * ChoicePicker component implementation
 *
 * Corresponds to the ChoicePicker component in the A2UI protocol.
 * Supports single-selection and multiple-selection modes.
 *
 * Supported properties:
 * - variant: variant type (String)
 *   - "mutuallyExclusive": single-selection mode
 *   - "multipleSelection": multiple-selection mode
 * - options: list of options (List<Map>)
 *   - label: option label (String)
 *   - value: option value (String)
 * - value: currently selected value (String or List<String>)
 * - displayStyle: display style (String)
 *   - "checkbox": checkbox list (default)
 *   - "chips": horizontal wrapping chip/pill tags
 * - filterable: show search input to filter options (boolean, default false)
 * - styles: style configuration (Map)
 *   - orientation: layout orientation for checkbox mode (String)
 *     - "vertical": vertical arrangement (default)
 *     - "horizontal": horizontal arrangement
 */
public class ChoicePickerComponent extends A2UIComponent {

    private static final String TAG = "ChoicePickerComponent";

    private LinearLayout containerLayout;
    private ViewGroup choiceContainer;
    private EditText filterEditText;
    private TextView errorTextView;
    private String variant = "mutuallyExclusive";
    private String orientation = "vertical";
    private String displayStyle = "checkbox";
    private boolean filterable = false;
    private String filterQuery = "";
    private List<Map<String, Object>> options;
    private List<View> radioButtons;
    private List<View> checkBoxes;
    private String selectedRadioValue;
    private List<String> selectedChipValues;
    private Map<String, String> styleConfig;

    public ChoicePickerComponent(String id, Map<String, Object> properties) {
        super(id, "ChoicePicker");
        this.checkBoxes = new ArrayList<>();
        this.radioButtons = new ArrayList<>();
    }

    @Override
    public View onCreateView(Context context) {
        containerLayout = new LinearLayout(context);
        containerLayout.setOrientation(LinearLayout.VERTICAL);

        styleConfig = ComponentStyleConfig.getInstance(context).getComponentStyle("ChoicePicker");
        parseProperties();
        filterQuery = "";

        if (filterable) {
            filterEditText = createFilterEditText(context);
            containerLayout.addView(filterEditText);
        }

        choiceContainer = buildChoiceContainer(context);
        LinearLayout.LayoutParams choiceParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        choiceContainer.setLayoutParams(choiceParams);
        containerLayout.addView(choiceContainer);

        rebuildChoices(context);

        errorTextView = new TextView(context);
        errorTextView.setTextColor(Color.RED);
        errorTextView.setTextSize(12);
        errorTextView.setVisibility(View.GONE);
        int errorPaddingH = StyleHelper.standardUnitToPx(context, 16);
        int errorPaddingV = StyleHelper.standardUnitToPx(context, 8);
        errorTextView.setPadding(errorPaddingH, errorPaddingV, errorPaddingH, 0);
        containerLayout.addView(errorTextView);

        applyChecksProperty();
        return containerLayout;
    }

    @Override
    public void onUpdateProperties(Map<String, Object> changedProps) {
        if (containerLayout == null) return;
        Context context = containerLayout.getContext();
        containerLayout.removeAllViews();
        errorTextView = null;

        styleConfig = ComponentStyleConfig.getInstance(context).getComponentStyle("ChoicePicker");
        parseProperties();
        filterQuery = "";

        if (filterable) {
            filterEditText = createFilterEditText(context);
            containerLayout.addView(filterEditText);
        } else {
            filterEditText = null;
        }

        choiceContainer = buildChoiceContainer(context);
        LinearLayout.LayoutParams choiceParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        choiceContainer.setLayoutParams(choiceParams);
        containerLayout.addView(choiceContainer);

        rebuildChoices(context);

        errorTextView = new TextView(context);
        errorTextView.setTextColor(Color.RED);
        errorTextView.setTextSize(12);
        errorTextView.setVisibility(View.GONE);
        int errorPaddingH = StyleHelper.standardUnitToPx(context, 16);
        int errorPaddingV = StyleHelper.standardUnitToPx(context, 8);
        errorTextView.setPadding(errorPaddingH, errorPaddingV, errorPaddingH, 0);
        containerLayout.addView(errorTextView);

        applyChecksProperty();
    }

    private void parseProperties() {
        if (properties.containsKey("variant")) {
            Object v = properties.get("variant");
            if (v instanceof String) variant = (String) v;
        }

        if (properties.containsKey("options")) {
            Object optionsObj = properties.get("options");
            if (optionsObj instanceof List) {
                options = (List<Map<String, Object>>) optionsObj;
            }
        }

        if (properties.containsKey("styles")) {
            Object stylesObj = properties.get("styles");
            if (stylesObj instanceof Map) {
                Map<String, Object> stylesMap = (Map<String, Object>) stylesObj;
                if (stylesMap.containsKey("orientation")) {
                    orientation = (String) stylesMap.get("orientation");
                }
            }
        }

        if (properties.containsKey("displayStyle")) {
            Object ds = properties.get("displayStyle");
            if (ds instanceof String) displayStyle = (String) ds;
        }

        if (properties.containsKey("filterable")) {
            Object f = properties.get("filterable");
            if (f instanceof Boolean) filterable = (Boolean) f;
        }

        if ("multipleSelection".equals(variant)) {
            Object valueObj = properties.get("value");
            if (valueObj instanceof List) {
                List<?> rawList = (List<?>) valueObj;
                selectedChipValues = new ArrayList<>(rawList.size());
                for (Object item : rawList) {
                    selectedChipValues.add(item != null ? item.toString() : "");
                }
            } else if (selectedChipValues == null) {
                selectedChipValues = new ArrayList<>();
            }
        } else {
            selectedRadioValue = getCurrentValue();
        }
    }

    /**
     * Build the choice container based on displayStyle.
     * - chips:  ChipsFlowLayout (custom row+wrap ViewGroup, no FlexboxLayout dependency)
     * - others: vertical/horizontal LinearLayout
     */
    private ViewGroup buildChoiceContainer(Context context) {
        if ("chips".equals(displayStyle)) {
            return new ChipsFlowLayout(context);
        } else {
            LinearLayout ll = new LinearLayout(context);
            ll.setOrientation("horizontal".equals(orientation)
                    ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
            return ll;
        }
    }

    private EditText createFilterEditText(Context context) {
        EditText et = new EditText(context);

        String hintText = styleConfig.get("filter-hint-text");
        et.setHint(hintText != null ? hintText : "Search...");

        String hintColorStr = styleConfig.get("filter-hint-color");
        et.setHintTextColor(hintColorStr != null ? StyleHelper.parseColor(hintColorStr) : 0x80000000);

        String textColorStr = styleConfig.get("filter-text-color");
        et.setTextColor(textColorStr != null ? StyleHelper.parseColor(textColorStr) : Color.BLACK);

        String textSizeStr = styleConfig.get("filter-text-size");
        int textSizePx = textSizeStr != null
                ? StyleHelper.parseDimension(textSizeStr, context)
                : StyleHelper.standardUnitToPx(context, 28);
        et.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSizePx);

        et.setSingleLine(true);

        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.RECTANGLE);
        String bgColorStr = styleConfig.get("filter-background-color");
        bg.setColor(bgColorStr != null ? StyleHelper.parseColor(bgColorStr) : Color.parseColor("#F5F5F5"));
        String borderColorStr = styleConfig.get("filter-border-color");
        String borderWidthStr = styleConfig.get("filter-border-width");
        int borderWidthPx = borderWidthStr != null
                ? StyleHelper.parseDimension(borderWidthStr, context)
                : StyleHelper.standardUnitToPx(context, 2);
        bg.setStroke(borderWidthPx, borderColorStr != null ? StyleHelper.parseColor(borderColorStr) : 0x1A000000);
        String radiusStr = styleConfig.get("filter-border-radius");
        float radiusPx = radiusStr != null
                ? StyleHelper.parseDimension(radiusStr, context)
                : StyleHelper.standardUnitToPx(context, 40);
        bg.setCornerRadius(radiusPx);
        et.setBackground(bg);

        String paddingHStr = styleConfig.get("filter-padding-h");
        String paddingVStr = styleConfig.get("filter-padding-v");
        int pH = paddingHStr != null
                ? StyleHelper.parseDimension(paddingHStr, context)
                : StyleHelper.standardUnitToPx(context, 32);
        int pV = paddingVStr != null
                ? StyleHelper.parseDimension(paddingVStr, context)
                : StyleHelper.standardUnitToPx(context, 16);
        et.setPadding(pH, pV, pH, pV);

        String marginBotStr = styleConfig.get("filter-margin-bottom");
        int marginBot = marginBotStr != null
                ? StyleHelper.parseDimension(marginBotStr, context)
                : StyleHelper.standardUnitToPx(context, 24);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = marginBot;
        et.setLayoutParams(params);

        et.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
            @Override public void afterTextChanged(Editable s) {
                filterQuery = s.toString().trim();
                rebuildChoices(context);
            }
        });

        return et;
    }

    private void rebuildChoices(Context context) {
        radioButtons.clear();
        checkBoxes.clear();
        choiceContainer.removeAllViews();

        List<Map<String, Object>> visible;
        if (filterable && !filterQuery.isEmpty() && options != null) {
            visible = new ArrayList<>();
            String lower = filterQuery.toLowerCase();
            for (Map<String, Object> opt : options) {
                Object lbl = opt.get("label");
                String lblStr = lbl instanceof String ? (String) lbl : "";
                if (lblStr.toLowerCase().contains(lower)) {
                    visible.add(opt);
                }
            }
        } else {
            visible = options != null ? options : new ArrayList<Map<String, Object>>();
        }

        if ("chips".equals(displayStyle)) {
            rebuildChips(context, visible);
        } else {
            rebuildCheckboxes(context, visible);
        }
    }

    private void rebuildChips(Context context, List<Map<String, Object>> visibleOptions) {
        List<String> currentValues = ("multipleSelection".equals(variant) && selectedChipValues != null)
                ? selectedChipValues : new ArrayList<String>();

        for (Map<String, Object> option : visibleOptions) {
            Object lblRaw = option.get("label");
            String label = lblRaw instanceof String ? (String) lblRaw : String.valueOf(lblRaw);
            Object valRaw = option.get("value");
            String value = valRaw instanceof String ? (String) valRaw : null;
            boolean isSelected = "multipleSelection".equals(variant)
                    ? currentValues.contains(value)
                    : value != null && value.equals(selectedRadioValue);

            TextView chip = createChipView(context, label, value, isSelected);
            chip.setOnClickListener(v -> {
                String clickedValue = (String) v.getTag();
                if ("multipleSelection".equals(variant)) {
                    if (selectedChipValues == null) selectedChipValues = new ArrayList<>();
                    if (selectedChipValues.contains(clickedValue)) {
                        selectedChipValues.remove(clickedValue);
                    } else {
                        selectedChipValues.add(clickedValue);
                    }
                    updateMultipleValues();
                } else {
                    selectedRadioValue = clickedValue;
                    updateValue(clickedValue);
                }
                rebuildChoices(context);
            });
            choiceContainer.addView(chip);
        }
    }

    private TextView createChipView(Context context, String label, String value, boolean isSelected) {
        String selBgStr   = styleConfig.get("chip-background-color-selected");
        String unselBgStr = styleConfig.get("chip-background-color");
        String selTxtStr  = styleConfig.get("chip-text-color-selected");
        String txtStr     = styleConfig.get("chip-text-color");
        String borderStr  = styleConfig.get("chip-border-color");
        String borderWStr = styleConfig.get("chip-border-width");
        String radiusStr  = styleConfig.get("chip-border-radius");
        String txtSizeStr = styleConfig.get("chip-text-size");
        String padHStr    = styleConfig.get("chip-padding-h");
        String padVStr    = styleConfig.get("chip-padding-v");
        String gapHStr    = styleConfig.get("chip-gap-h");
        String gapVStr    = styleConfig.get("chip-gap-v");

        int selBgColor    = selBgStr   != null ? StyleHelper.parseColor(selBgStr)   : Color.parseColor("#2E82FF");
        int unselBgColor  = unselBgStr != null ? StyleHelper.parseColor(unselBgStr) : Color.parseColor("#F5F5F5");
        int selTxtColor   = selTxtStr  != null ? StyleHelper.parseColor(selTxtStr)  : Color.WHITE;
        int txtColor      = txtStr     != null ? StyleHelper.parseColor(txtStr)     : Color.BLACK;
        int borderColor   = borderStr  != null ? StyleHelper.parseColor(borderStr)  : 0x1A000000;
        int borderWidthPx = borderWStr != null
                ? StyleHelper.parseDimension(borderWStr, context)
                : StyleHelper.standardUnitToPx(context, 2);
        float radiusPx    = radiusStr  != null
                ? StyleHelper.parseDimension(radiusStr, context)
                : StyleHelper.standardUnitToPx(context, 40);
        int txtSizePx     = txtSizeStr != null
                ? StyleHelper.parseDimension(txtSizeStr, context)
                : StyleHelper.standardUnitToPx(context, 28);
        int padH = padHStr != null
                ? StyleHelper.parseDimension(padHStr, context)
                : StyleHelper.standardUnitToPx(context, 24);
        int padV = padVStr != null
                ? StyleHelper.parseDimension(padVStr, context)
                : StyleHelper.standardUnitToPx(context, 12);
        int gapH = gapHStr != null
                ? StyleHelper.parseDimension(gapHStr, context)
                : StyleHelper.standardUnitToPx(context, 16);
        int gapV = gapVStr != null
                ? StyleHelper.parseDimension(gapVStr, context)
                : StyleHelper.standardUnitToPx(context, 16);

        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.RECTANGLE);
        bg.setCornerRadius(radiusPx);
        if (isSelected) {
            bg.setColor(selBgColor);
        } else {
            bg.setColor(unselBgColor);
            bg.setStroke(borderWidthPx, borderColor);
        }

        TextView chip = new TextView(context);
        chip.setTag(value);
        chip.setText(isSelected ? label + " ×" : label);
        chip.setTextColor(isSelected ? selTxtColor : txtColor);
        chip.setTextSize(TypedValue.COMPLEX_UNIT_PX, txtSizePx);
        chip.setPadding(padH, padV, padH, padV);
        chip.setBackground(bg);

        // Use MarginLayoutParams so ChipsFlowLayout can pick up gap-h / gap-v as margins
        ViewGroup.MarginLayoutParams lp = new ViewGroup.MarginLayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMarginEnd(gapH);
        lp.bottomMargin = gapV;
        chip.setLayoutParams(lp);

        return chip;
    }

    private void rebuildCheckboxes(Context context, List<Map<String, Object>> visibleOptions) {
        if ("multipleSelection".equals(variant)) {
            List<String> currentValues = getCurrentValues();
            int index = 0;
            for (Map<String, Object> option : visibleOptions) {
                String label = (String) option.get("label");
                String value = (String) option.get("value");

                LinearLayout itemLayout = new LinearLayout(context);
                itemLayout.setOrientation(LinearLayout.HORIZONTAL);
                itemLayout.setGravity(Gravity.CENTER_VERTICAL);
                int paddingH = StyleHelper.standardUnitToPx(context, 24);
                int paddingV = StyleHelper.standardUnitToPx(context, 22);
                itemLayout.setPadding(paddingH, paddingV, paddingH, paddingV);
                itemLayout.setTag(value);

                CustomCheckBoxView checkBoxView = new CustomCheckBoxView(context);
                applyCheckBoxStyle(context, checkBoxView);
                checkBoxView.setChecked(currentValues != null && currentValues.contains(value));

                TextView textView = new TextView(context);
                textView.setText(label);
                applyOptionTextStyle(context, textView);

                itemLayout.addView(checkBoxView);
                itemLayout.addView(textView);

                itemLayout.setOnClickListener(v -> {
                    checkBoxView.toggle();
                    updateMultipleValues();
                });

                checkBoxes.add(itemLayout);

                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT);
                if (index > 0) {
                    applyChoiceGap(context, params);
                }
                itemLayout.setLayoutParams(params);

                choiceContainer.addView(itemLayout);
                index++;
            }
        } else {
            String currentValue = getCurrentValue();
            selectedRadioValue = currentValue;

            for (int i = 0; i < visibleOptions.size(); i++) {
                Map<String, Object> option = visibleOptions.get(i);
                String label = (String) option.get("label");
                String value = (String) option.get("value");

                LinearLayout itemLayout = new LinearLayout(context);
                itemLayout.setOrientation(LinearLayout.HORIZONTAL);
                itemLayout.setGravity(Gravity.CENTER_VERTICAL);
                int paddingH = StyleHelper.standardUnitToPx(context, 24);
                int paddingV = StyleHelper.standardUnitToPx(context, 22);
                itemLayout.setPadding(paddingH, paddingV, paddingH, paddingV);
                itemLayout.setTag(value);

                CustomCheckBoxView radioView = new CustomCheckBoxView(context);
                applyCheckBoxStyle(context, radioView);
                radioView.setChecked(value != null && value.equals(currentValue));

                TextView textView = new TextView(context);
                textView.setText(label);
                applyOptionTextStyle(context, textView);

                itemLayout.addView(radioView);
                itemLayout.addView(textView);

                itemLayout.setOnClickListener(v -> {
                    String clickedValue = (String) v.getTag();
                    for (View item : radioButtons) {
                        LinearLayout layout = (LinearLayout) item;
                        CustomCheckBoxView radio = (CustomCheckBoxView) layout.getChildAt(0);
                        String itemValue = (String) layout.getTag();
                        radio.setChecked(itemValue != null && itemValue.equals(clickedValue));
                    }
                    selectedRadioValue = clickedValue;
                    updateValue(clickedValue);
                });

                radioButtons.add(itemLayout);

                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT);
                if (i > 0) {
                    applyChoiceGap(context, params);
                }
                itemLayout.setLayoutParams(params);

                choiceContainer.addView(itemLayout);
            }
        }
    }

    private void applyOptionTextStyle(Context context, TextView textView) {
        String textSizeStr = styleConfig.get("text-size");
        int textSizePx = textSizeStr != null
                ? StyleHelper.parseDimension(textSizeStr, context)
                : StyleHelper.standardUnitToPx(context, 32);
        textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSizePx);

        String textColorStr = styleConfig.get("text-color");
        int textColor = textColorStr != null ? StyleHelper.parseColor(textColorStr) : Color.BLACK;
        textView.setTextColor(textColor);

        String textMarginStr = styleConfig.get("text-margin");
        int textLeftMargin = textMarginStr != null
                ? StyleHelper.parseDimension(textMarginStr, context)
                : StyleHelper.standardUnitToPx(context, 16);
        textView.setPadding(textLeftMargin, 0, 0, 0);

        LinearLayout.LayoutParams textParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        textView.setLayoutParams(textParams);
    }

    private void applyChoiceGap(Context context, LinearLayout.LayoutParams params) {
        String choiceGapStr = styleConfig.get("choice-gap");
        if (choiceGapStr != null && !choiceGapStr.isEmpty()) {
            int choiceGap = StyleHelper.parseDimension(choiceGapStr, context);
            if ("horizontal".equals(orientation)) {
                params.leftMargin = choiceGap;
            } else {
                params.topMargin = choiceGap;
            }
        }
    }

    private void applyChecksProperty() {
        if (properties.containsKey("checks")) {
            Object checksValue = properties.get("checks");
            if (checksValue instanceof Map) {
                @SuppressWarnings("unchecked")
                Map<String, Object> checksMap = (Map<String, Object>) checksValue;
                Object resultObj = checksMap.get("result");
                boolean result = resultObj instanceof Boolean ? (Boolean) resultObj : true;
                String message = checksMap.containsKey("message") ?
                        String.valueOf(checksMap.get("message")) : "";

                if (errorTextView != null) {
                    errorTextView.setText(result ? "" : message);
                    errorTextView.setVisibility(result ? View.GONE : View.VISIBLE);
                }
                if (choiceContainer != null) {
                    choiceContainer.setEnabled(result);
                    choiceContainer.setAlpha(result ? 1.0f : 0.5f);
                }
            }
        }
    }

    private void applyCheckBoxStyle(Context context, CustomCheckBoxView checkBoxView) {
        String sizeStr = styleConfig.get("checkbox-size");
        if (sizeStr != null) {
            int size = StyleHelper.parseDimension(sizeStr, context);
            checkBoxView.setSize(size);
        }

        String bgColorSelectedStr = styleConfig.get("checkbox-background-color-selected");
        if (bgColorSelectedStr != null) {
            checkBoxView.setCheckedBackgroundColor(StyleHelper.parseColor(bgColorSelectedStr));
        }

        String borderColorSelectedStr = styleConfig.get("checkbox-border-color-selected");
        if (borderColorSelectedStr != null) {
            checkBoxView.setCheckedBorderColor(StyleHelper.parseColor(borderColorSelectedStr));
        }

        String bgColorStr = styleConfig.get("checkbox-background-color");
        if (bgColorStr != null) {
            checkBoxView.setUncheckedBackgroundColor(StyleHelper.parseColor(bgColorStr));
        }

        String borderColorStr = styleConfig.get("checkbox-border-color");
        if (borderColorStr != null) {
            checkBoxView.setUncheckedBorderColor(StyleHelper.parseColor(borderColorStr));
        }

        String borderWidthStr = styleConfig.get("checkbox-border-width");
        if (borderWidthStr != null) {
            checkBoxView.setBorderWidth(StyleHelper.parseDimension(borderWidthStr, context));
        }

        String borderRadiusStr = styleConfig.get("checkbox-border-radius");
        if (borderRadiusStr != null) {
            checkBoxView.setCornerRadius(StyleHelper.parseDimension(borderRadiusStr, context));
        }
    }

    private String getCurrentValue() {
        if (properties.containsKey("value")) {
            Object value = properties.get("value");
            if (value instanceof String) {
                return (String) value;
            }
        }
        return null;
    }

    private List<String> getCurrentValues() {
        if (properties.containsKey("value")) {
            Object value = properties.get("value");
            if (value instanceof List) {
                return (List<String>) value;
            }
        }
        return new ArrayList<>();
    }

    private void updateValue(String newValue) {
        properties.put("value", newValue);
        sendDataChangeToNative(newValue);
    }

    private void updateMultipleValues() {
        List<String> selectedValues;
        if ("chips".equals(displayStyle)) {
            selectedValues = selectedChipValues != null
                    ? new ArrayList<>(selectedChipValues) : new ArrayList<String>();
        } else {
            selectedValues = new ArrayList<>();
            for (View item : checkBoxes) {
                LinearLayout layout = (LinearLayout) item;
                CustomCheckBoxView checkBoxView = (CustomCheckBoxView) layout.getChildAt(0);
                if (checkBoxView.isChecked()) {
                    String value = (String) layout.getTag();
                    if (value != null) {
                        selectedValues.add(value);
                    }
                }
            }
        }
        properties.put("value", selectedValues);
        sendDataChangeToNative(selectedValues);
    }

    private void sendDataChangeToNative(String value) {
        try {
            JSONObject json = new JSONObject();
            json.put("value", value);
            syncState(json.toString());
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "sendDataChangeToNative: failed to build JSON", e);
        }
    }

    private void sendDataChangeToNative(List<String> values) {
        try {
            JSONArray array = new JSONArray();
            for (String v : values) {
                array.put(v);
            }
            JSONObject json = new JSONObject();
            json.put("value", array);
            syncState(json.toString());
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "sendDataChangeToNative: failed to build JSON", e);
        }
    }

    /**
     * Lightweight flow layout that lays children out in left-to-right rows
     * and wraps to the next row when the next child would exceed the parent's
     * available width. Honors per-child {@link MarginLayoutParams} margins.
     *
     * Replaces {@code com.google.android.flexbox.FlexboxLayout} so the project
     * does not need to add a third-party dependency just for the chips layout.
     */
    private static class ChipsFlowLayout extends ViewGroup {
        ChipsFlowLayout(Context context) {
            super(context);
        }

        @Override
        protected LayoutParams generateDefaultLayoutParams() {
            return new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        }

        @Override
        public LayoutParams generateLayoutParams(LayoutParams p) {
            return new MarginLayoutParams(p);
        }

        @Override
        protected boolean checkLayoutParams(LayoutParams p) {
            return p instanceof MarginLayoutParams;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int widthSize = MeasureSpec.getSize(widthMeasureSpec);
            int widthMode = MeasureSpec.getMode(widthMeasureSpec);
            int heightMode = MeasureSpec.getMode(heightMeasureSpec);
            int heightSize = MeasureSpec.getSize(heightMeasureSpec);

            int paddingH = getPaddingLeft() + getPaddingRight();
            int paddingV = getPaddingTop() + getPaddingBottom();
            int availableWidth = Math.max(0, widthSize - paddingH);

            int rowWidth = 0;
            int rowHeight = 0;
            int totalHeight = 0;
            int maxRowWidth = 0;
            boolean firstChildInRow = true;

            for (int i = 0; i < getChildCount(); i++) {
                View child = getChildAt(i);
                if (child.getVisibility() == GONE) continue;

                MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
                measureChildWithMargins(child, widthMeasureSpec, paddingH, heightMeasureSpec, paddingV);
                int childW = child.getMeasuredWidth() + lp.leftMargin + lp.rightMargin;
                int childH = child.getMeasuredHeight() + lp.topMargin + lp.bottomMargin;

                if (!firstChildInRow && rowWidth + childW > availableWidth) {
                    // Wrap to next row
                    totalHeight += rowHeight;
                    maxRowWidth = Math.max(maxRowWidth, rowWidth);
                    rowWidth = childW;
                    rowHeight = childH;
                } else {
                    rowWidth += childW;
                    rowHeight = Math.max(rowHeight, childH);
                }
                firstChildInRow = false;
            }
            totalHeight += rowHeight;
            maxRowWidth = Math.max(maxRowWidth, rowWidth);

            int finalWidth = (widthMode == MeasureSpec.EXACTLY)
                    ? widthSize
                    : Math.min(maxRowWidth + paddingH, widthSize);
            int desiredHeight = totalHeight + paddingV;
            int finalHeight = (heightMode == MeasureSpec.EXACTLY)
                    ? heightSize
                    : (heightMode == MeasureSpec.AT_MOST
                        ? Math.min(desiredHeight, heightSize)
                        : desiredHeight);

            setMeasuredDimension(finalWidth, finalHeight);
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            int paddingLeft = getPaddingLeft();
            int paddingTop = getPaddingTop();
            int paddingRight = getPaddingRight();
            int width = r - l;
            int availableWidth = width - paddingLeft - paddingRight;

            // Pass 1: group children into rows and remember each row's used width
            // so we can horizontally center each row in pass 2.
            int childCount = getChildCount();
            int[] rowStartIndex = new int[childCount];
            int[] rowChildCount = new int[childCount];
            int[] rowWidth = new int[childCount];
            int[] rowHeights = new int[childCount];
            int rowCount = 0;
            int currentRowWidth = 0;
            int currentRowHeight = 0;
            int currentRowStart = 0;
            int currentRowChildren = 0;

            for (int i = 0; i < childCount; i++) {
                View child = getChildAt(i);
                if (child.getVisibility() == GONE) continue;
                MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
                int childTotalW = child.getMeasuredWidth() + lp.leftMargin + lp.rightMargin;
                int childTotalH = child.getMeasuredHeight() + lp.topMargin + lp.bottomMargin;

                if (currentRowChildren > 0 && currentRowWidth + childTotalW > availableWidth) {
                    rowStartIndex[rowCount] = currentRowStart;
                    rowChildCount[rowCount] = currentRowChildren;
                    rowWidth[rowCount] = currentRowWidth;
                    rowHeights[rowCount] = currentRowHeight;
                    rowCount++;
                    currentRowStart = i;
                    currentRowWidth = childTotalW;
                    currentRowHeight = childTotalH;
                    currentRowChildren = 1;
                } else {
                    if (currentRowChildren == 0) {
                        currentRowStart = i;
                    }
                    currentRowWidth += childTotalW;
                    currentRowHeight = Math.max(currentRowHeight, childTotalH);
                    currentRowChildren++;
                }
            }
            if (currentRowChildren > 0) {
                rowStartIndex[rowCount] = currentRowStart;
                rowChildCount[rowCount] = currentRowChildren;
                rowWidth[rowCount] = currentRowWidth;
                rowHeights[rowCount] = currentRowHeight;
                rowCount++;
            }

            // Pass 2: lay out each row centered horizontally inside availableWidth.
            int currentY = paddingTop;
            for (int row = 0; row < rowCount; row++) {
                int rowOffset = paddingLeft + Math.max(0, (availableWidth - rowWidth[row]) / 2);
                int currentX = rowOffset;

                int start = rowStartIndex[row];
                int end = start + rowChildCount[row];
                for (int i = start; i < end; i++) {
                    View child = getChildAt(i);
                    if (child.getVisibility() == GONE) continue;
                    MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
                    int childW = child.getMeasuredWidth();
                    int childH = child.getMeasuredHeight();
                    int childLeft = currentX + lp.leftMargin;
                    int childTop = currentY + lp.topMargin;
                    child.layout(childLeft, childTop, childLeft + childW, childTop + childH);
                    currentX += childW + lp.leftMargin + lp.rightMargin;
                }

                currentY += rowHeights[row];
            }
        }
    }
}
