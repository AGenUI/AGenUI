package com.amap.agenui.render.style;

import android.view.Gravity;
import android.view.View;

import java.util.Locale;
import java.util.Map;

/**
 * 文本组件的方向语义翻译层：把「页面有效方向 + 原始 text-align 语义」翻译成物理
 * gravity 与 view-relative 文本方向。
 *
 * <p>方向一律来自调用方传入的组件快照，本类不读 App Locale、不访问 Surface、
 * 不持有 View 或 Context 静态引用。
 *
 * <p>start/end 依据页面方向映射到物理左右；物理 left/right 恒定不变，两者不可混同。
 */
public final class TextDirectionSupport {

    /** 组件快照顶层的方向键；缺失 / null / 非法值一律按 LTR 处理。 */
    public static final String DIRECTION_KEY = "writingDirection";

    public static final int LTR = 0;
    public static final int RTL = 1;

    /** text-align 水平语义，保留 start/end 与 left/right 的区别。 */
    public static final int H_ALIGN_START = 0;
    public static final int H_ALIGN_END = 1;
    public static final int H_ALIGN_LEFT = 2;
    public static final int H_ALIGN_RIGHT = 3;
    public static final int H_ALIGN_CENTER = 4;

    private TextDirectionSupport() {
    }

    // ======================== 方向输入 ========================

    /**
     * 解析快照里的方向值。只接受字符串 {@code "rtl"}（大小写不敏感）；
     * 其余情况（缺失、null、非法值、非字符串）一律 LTR。
     */
    public static int resolveDirection(Object raw) {
        if (raw instanceof String) {
            return "rtl".equals(((String) raw).trim().toLowerCase(Locale.ROOT)) ? RTL : LTR;
        }
        return LTR;
    }

    public static int resolveDirection(Map<String, Object> props) {
        return props == null ? LTR : resolveDirection(props.get(DIRECTION_KEY));
    }

    public static boolean isRtl(int direction) {
        return direction == RTL;
    }

    /**
     * 将解析后的方向映射为 Android TextView 的文本书写方向。
     * 输入 RTL 则为 View.TEXT_DIRECTION_RTL；输入 LTR 或其他（默认）则为 View.TEXT_DIRECTION_LTR。
     */
    public static int viewTextDirection(int direction) {
        return isRtl(direction) ? View.TEXT_DIRECTION_RTL : View.TEXT_DIRECTION_LTR;
    }

    // ======================== text-align 语义 ========================

    /** 解析水平语义；未声明或非法值按 start 处理。 */
    public static int parseHorizontalAlign(Object textAlign) {
        String first = firstToken(textAlign);
        if (first == null) {
            return H_ALIGN_START;
        }
        switch (first) {
            case "center": return H_ALIGN_CENTER;
            case "left":   return H_ALIGN_LEFT;
            case "right":  return H_ALIGN_RIGHT;
            case "end":    return H_ALIGN_END;
            case "start":
            default:       return H_ALIGN_START;
        }
    }

    /** 物理水平 gravity：start/end 按页面方向映射，left/right 恒定。 */
    public static int physicalHorizontalGravity(int hAlign, int direction) {
        switch (hAlign) {
            case H_ALIGN_CENTER: return Gravity.CENTER_HORIZONTAL;
            case H_ALIGN_LEFT:   return Gravity.LEFT;
            case H_ALIGN_RIGHT:  return Gravity.RIGHT;
            case H_ALIGN_END:    return isRtl(direction) ? Gravity.LEFT : Gravity.RIGHT;
            case H_ALIGN_START:
            default:             return isRtl(direction) ? Gravity.RIGHT : Gravity.LEFT;
        }
    }

    private static String firstToken(Object textAlign) {
        if (textAlign == null) {
            return null;
        }
        String[] parts = String.valueOf(textAlign).trim().toLowerCase(Locale.ROOT).split("\\s+");
        return parts.length == 0 || parts[0].isEmpty() ? null : parts[0];
    }

}
