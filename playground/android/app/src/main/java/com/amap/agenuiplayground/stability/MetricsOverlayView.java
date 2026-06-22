package com.amap.agenuiplayground.stability;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.Locale;

/**
 * Floating semi-transparent overlay showing real-time stability test metrics.
 * Displays scenario, round count, timer, memory usage, and error count.
 */
public class MetricsOverlayView extends FrameLayout {
    private final TextView tvScenario;
    private final TextView tvRound;
    private final TextView tvTimer;
    private final TextView tvMemory;
    private final TextView tvStatus;

    public MetricsOverlayView(Context context) {
        super(context);

        // Background: semi-transparent black with rounded corners
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.argb(179, 0, 0, 0)); // 0.7 alpha
        bg.setCornerRadius(dp(12));
        setBackground(bg);
        int pad = dp(10);
        setPadding(dp(12), pad, dp(12), pad);

        // Vertical layout for labels
        LinearLayout stack = new LinearLayout(context);
        stack.setOrientation(LinearLayout.VERTICAL);
        stack.setLayoutParams(new FrameLayout.LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        tvScenario = createLabel(context);
        tvRound = createLabel(context);
        tvTimer = createLabel(context);
        tvMemory = createLabel(context);
        tvStatus = createLabel(context);
        tvStatus.setTextColor(Color.parseColor("#4CFF4C"));

        stack.addView(tvScenario);
        stack.addView(tvRound);
        stack.addView(tvTimer);
        stack.addView(tvMemory);
        stack.addView(tvStatus);

        addView(stack);

        // Default values
        tvScenario.setText("IDLE");
        tvRound.setText("R:0");
        tvTimer.setText("00:00:00");
        tvMemory.setText("0 MB");
        tvStatus.setText("READY");
    }

    /**
     * Update all displayed metrics.
     */
    public void update(String scenario, int round, int maxRounds,
                       long elapsedMs, int memoryMb, int peakMb,
                       int errors, boolean isRunning) {
        int h = (int) (elapsedMs / 3600000);
        int m = (int) ((elapsedMs % 3600000) / 60000);
        int s = (int) ((elapsedMs % 60000) / 1000);

        tvScenario.setText(abbreviate(scenario));
        tvRound.setText(maxRounds > 0
                ? String.format(Locale.US, "R:%d/%d", round, maxRounds)
                : String.format(Locale.US, "R:%d", round));
        tvTimer.setText(String.format(Locale.US, "%02d:%02d:%02d", h, m, s));
        tvMemory.setText(String.format(Locale.US, "%dMB (pk:%d)", memoryMb, peakMb));

        if (errors > 0) {
            tvStatus.setText(String.format(Locale.US, "ERR:%d", errors));
            tvStatus.setTextColor(Color.parseColor("#FF6666"));
        } else {
            tvStatus.setText(isRunning ? "RUNNING" : "DONE");
            tvStatus.setTextColor(isRunning
                    ? Color.parseColor("#4CFF4C")
                    : Color.parseColor("#FFCC33"));
        }
    }

    private String abbreviate(String scenario) {
        if (scenario == null) return "IDLE";
        String s = scenario.replace("REALISTIC_", "R:");
        return s.length() > 16 ? s.substring(0, 16) : s;
    }

    private TextView createLabel(Context context) {
        TextView tv = new TextView(context);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tv.setTextColor(Color.WHITE);
        tv.setTypeface(Typeface.MONOSPACE, Typeface.NORMAL);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(2);
        tv.setLayoutParams(lp);
        return tv;
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, value,
                getContext().getResources().getDisplayMetrics());
    }
}
