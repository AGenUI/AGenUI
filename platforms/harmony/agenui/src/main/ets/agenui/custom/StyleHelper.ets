import nativeModule from 'liba2ui-capi.so';
import type { EdgeInsetsValue, EdgeInsetSideValue } from 'liba2ui-capi.so';

/** 四边 vp。a2ui = 2 × vp，见 resolveSideVp。 */
export interface EdgeInsetsVp {
  top: number;
  right: number;
  bottom: number;
  left: number;
}

/** EdgeInsetSideValue.unit === 0 时才是 px（core 把无单位数字也归一到 px） */
const UNIT_PX: number = 0;
const A2UI_TO_VP: number = 0.5;
const WRAP_CONTENT: number = -2;
const MATCH_PARENT: number = -1;

const NAMED_COLORS: Map<string, number> = new Map<string, number>([
  ['transparent', 0x00000000],
  ['black', 0xFF000000],
  ['white', 0xFFFFFFFF],
  ['red', 0xFFFF0000],
  ['green', 0xFF00FF00],
  ['blue', 0xFF0000FF],
  ['yellow', 0xFFFFFF00],
  ['gray', 0xFF808080],
  ['grey', 0xFF808080]
]);

export class StyleHelper {
  /**
   * Parses a CSS color into ARGB. The return value is always a number
   * (historical number-only contract kept for existing callers): parse
   * failure / gradient map to 0, and a legal explicit transparent
   * ('transparent' / rgba(0,0,0,0)) is also 0 -- the two cannot be told
   * apart here; use tryParseColor when the caller must distinguish them.
   * 'currentcolor' parses as a solid placeholder (0xFF000000), aligned with
   * iOS/Android.
   */
  public static parseColor(value: Object): number {
    const parsed: number | undefined = StyleHelper.tryParseColor(value);
    return parsed === undefined ? 0x00000000 : parsed;
  }

  /**
   * Distinguishing variant of parseColor: returns the ARGB number on success
   * (a legal explicit transparent such as 'transparent' yields 0), and
   * undefined when the value cannot be resolved to a solid color (parse
   * failure / gradient / non-string / empty input). 'currentcolor' counts as
   * a successful solid (0xFF000000 placeholder), aligned with iOS/Android.
   */
  public static tryParseColor(value: Object): number | undefined {
    if (typeof value === 'number') {
      return value;
    }
    if (typeof value !== 'string') {
      return undefined;
    }

    const raw = value.trim();
    if (raw.length === 0) {
      return undefined;
    }

    // Prefer the shared C++ core ColorParser (NAPI) so color semantics stay
    // aligned with Android/iOS. The ArkTS implementation below is only a
    // fallback for environments where the native symbol is unavailable.
    try {
      if (typeof nativeModule.parseColor === 'function') {
        const parsed = nativeModule.parseColor(raw);
        if (typeof parsed === 'number') {
          return parsed >>> 0;
        }
        // Native answered with undefined: parse failure / gradient.
        // Report the failure instead of falling back.
        return undefined;
      }
    } catch (_error) {
      // Fall through to the ArkTS fallback.
    }

    // Version-mismatch degradation: the ArkTS fallback cannot distinguish a
    // parse failure from a legal explicit transparent (both yield 0), so 0 is
    // treated as failure here. Normal builds take the NAPI path above.
    const fallback = StyleHelper.parseColorFallback(value);
    return fallback === 0 ? undefined : fallback;
  }

  // Degradation path for NAPI-missing environments only (version mismatch,
  // no native .so). Coupled with tryParseColor: a 0 returned here is treated
  // as "unresolvable", so this path intentionally cannot report a legal
  // explicit transparent.
  private static parseColorFallback(value: Object): number {
    if (typeof value === 'number') {
      return value;
    }
    if (typeof value !== 'string') {
      return 0x00000000;
    }

    const raw = value.trim();
    if (raw.length === 0) {
      return 0x00000000;
    }

    const named = NAMED_COLORS.get(raw.toLowerCase());
    if (named !== undefined) {
      return named;
    }

    if (raw.startsWith('#')) {
      const hex = raw.slice(1);
      if (hex.length === 6) {
        return Number.parseInt(`FF${hex}`, 16);
      }
      if (hex.length === 8) {
        const rgba = hex.toUpperCase();
        return Number.parseInt(`${rgba.slice(6, 8)}${rgba.slice(0, 6)}`, 16);
      }
    }

    const rgbaMatch = raw.match(new RegExp('^rgba?\\((.+)\\)$', 'i'));
    if (rgbaMatch && rgbaMatch[1]) {
      const parts = rgbaMatch[1].split(',').map((part: string) => part.trim());
      if (parts.length === 3 || parts.length === 4) {
        const r = Math.max(0, Math.min(255, Number.parseInt(parts[0], 10) || 0));
        const g = Math.max(0, Math.min(255, Number.parseInt(parts[1], 10) || 0));
        const b = Math.max(0, Math.min(255, Number.parseInt(parts[2], 10) || 0));
        const alpha = parts.length === 4 ? Number.parseFloat(parts[3]) : 1;
        const a = Math.max(0, Math.min(255, Math.round(alpha * 255)));
        return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
      }
    }

    return 0x00000000;
  }

  public static parseDimension(value: Object): number {
    if (typeof value === 'number') {
      return value;
    }
    if (typeof value !== 'string') {
      return 0;
    }

    const raw = value.trim().toLowerCase();
    if (raw.length === 0) {
      return 0;
    }
    if (raw === 'auto' || raw === 'wrap_content') {
      return -2;
    }
    if (raw === 'match_parent' || raw === '100%') {
      return -1;
    }
    if (raw.endsWith('px')) {
      return Number.parseFloat(raw.slice(0, -2)) || 0;
    }
    return Number.parseFloat(raw) || 0;
  }

  // ------------------------------------------------------------- edge insets
  //
  // 下面这组走 native `parseEdgeInsets`（core `agenui::EdgeInsetsParser`），
  // 与 Android 的 jni_edge_insets_parser / iOS 的直接链接是同一份实现，
  // 因此 CSS 简写文法三端一致。**不要在 ArkTS 里另写一套简写解析**。

  /**
   * DSL 尺寸 → a2ui。
   *
   * 与 parseDimension 的差别（都要保留，语义不同）：本方法把「非字符串 / 空串 /
   * 非 px 单位 / calc()」统一收敛为布局哨兵 WRAP_CONTENT(-2)，让调用方用 `> 0`
   * 一次判掉所有无效输入；parseDimension 对这些情况返回 0，且对 `"1.5em"` 之类
   * 会做前缀解析得到 1.5（JS parseFloat 语义），把无效值当成了有效尺寸。
   */
  public static parseDimensionA2ui(value: Object | undefined | null): number {
    if (typeof value === 'number') {
      return value as number;
    }
    if (typeof value !== 'string') {
      return WRAP_CONTENT;
    }
    const css: string = (value as string).trim().toLowerCase();
    if (css.length === 0) {
      return WRAP_CONTENT;
    }
    if (css === 'auto' || css === 'wrap_content') {
      return WRAP_CONTENT;
    }
    if (css === 'match_parent' || css === '100%') {
      return MATCH_PARENT;
    }
    const insets: EdgeInsetsValue | null = StyleHelper.nativeParseEdgeInsets(css);
    if (insets === null) {
      return WRAP_CONTENT;
    }
    const side: EdgeInsetSideValue = insets.top;
    if (side.isCalc || side.unit !== UNIT_PX) {
      return WRAP_CONTENT;
    }
    return side.value;
  }

  /** 单个 CSS 长度 → vp。无效输入一律 0，布局哨兵不会漏进几何计算。 */
  public static parseDimensionVp(value: Object | undefined | null): number {
    const a2ui: number = StyleHelper.parseDimensionA2ui(value);
    return a2ui > 0 ? a2ui * A2UI_TO_VP : 0;
  }

  /**
   * `padding` 简写 + `padding-*` 长写 → 四边 vp。
   *
   * ⚠️ 与 resolveCSSMarginVp **刻意不对称**：空 styles 在这里得到零 Rect，
   * 在 margin 那边得到 null。Android StyleHelper.resolveCSSPaddingPx 就是这样
   * （Rect 在空检查之前就分配了，几乎肯定是笔误），复刻是为了让从 Android 移植
   * 过来的代码分支一致。**不要单端"修好"它。**
   *
   * styles 非空但不含任何 padding* 键时返回 null，调用方据此区分「未声明」与「显式 0」。
   */
  public static resolveCSSPaddingVp(styles: Record<string, Object> | null | undefined): EdgeInsetsVp | null {
    return StyleHelper.resolveEdges(styles, 'padding', true);
  }

  /** `margin` 简写 + `margin-*` 长写 → 四边 vp。空 styles 得到 null，见上。 */
  public static resolveCSSMarginVp(styles: Record<string, Object> | null | undefined): EdgeInsetsVp | null {
    return StyleHelper.resolveEdges(styles, 'margin', false);
  }

  /** @param zeroWhenEmpty padding 传 true、margin 传 false（Android 的不对称） */
  private static resolveEdges(styles: Record<string, Object> | null | undefined,
                              prefix: string,
                              zeroWhenEmpty: boolean): EdgeInsetsVp | null {
    const zero: EdgeInsetsVp = { top: 0, right: 0, bottom: 0, left: 0 };
    if (styles === null || styles === undefined) {
      return zeroWhenEmpty ? zero : null;
    }
    if (Object.keys(styles).length === 0) {
      return zeroWhenEmpty ? zero : null;
    }

    let top: number = 0;
    let right: number = 0;
    let bottom: number = 0;
    let left: number = 0;
    let present: boolean = false;

    const shorthand: Object | undefined = styles[prefix];
    if (shorthand !== undefined && shorthand !== null) {
      const raw: string = `${shorthand}`.trim();
      // Android 会跳过字面量 "null"（上游把 JSON null 字符串化后会出现）
      if (raw.length !== 0 && raw.toLowerCase() !== 'null') {
        const insets: EdgeInsetsValue | null = StyleHelper.nativeParseEdgeInsets(raw);
        if (insets !== null) {
          top = StyleHelper.resolveSideVp(insets.top);
          right = StyleHelper.resolveSideVp(insets.right);
          bottom = StyleHelper.resolveSideVp(insets.bottom);
          left = StyleHelper.resolveSideVp(insets.left);
          present = true;
        }
      }
    }

    // 长写覆盖简写（CSS 层叠）。判据是键存在，不看值是否有效
    if (styles[`${prefix}-top`] !== undefined) {
      top = StyleHelper.parseDimensionVp(styles[`${prefix}-top`]);
      present = true;
    }
    if (styles[`${prefix}-right`] !== undefined) {
      right = StyleHelper.parseDimensionVp(styles[`${prefix}-right`]);
      present = true;
    }
    if (styles[`${prefix}-bottom`] !== undefined) {
      bottom = StyleHelper.parseDimensionVp(styles[`${prefix}-bottom`]);
      present = true;
    }
    if (styles[`${prefix}-left`] !== undefined) {
      left = StyleHelper.parseDimensionVp(styles[`${prefix}-left`]);
      present = true;
    }

    if (!present) {
      return null;
    }
    // 负值逐边钳 0，与 Android 一致
    return {
      top: top < 0 ? 0 : top,
      right: right < 0 ? 0 : right,
      bottom: bottom < 0 ? 0 : bottom,
      left: left < 0 ? 0 : left,
    };
  }

  /**
   * 一条已解析的边 → vp。
   *
   * 只认 px（含被 core 归一成 px 的无单位值），calc() 与其余 13 个单位一律 0 ——
   * 与 Android StyleHelper.resolveSidePx 及鸿蒙 C++ 侧 a2ui_padding_utils.h 同一口径。
   * 不取整、不做发丝线钳位：vp 本身是浮点、density 由 ArkUI 施加，
   * 在这里取整会与 C++ 侧脱钩；发丝线保护属于 border/装饰线的绘制处。
   */
  private static resolveSideVp(side: EdgeInsetSideValue | undefined): number {
    if (side === undefined || side === null || side.isCalc) {
      return 0;
    }
    if (side.unit !== UNIT_PX) {
      return 0;
    }
    return side.value * A2UI_TO_VP;
  }

  /** native 符号缺失（版本不匹配 / 无 .so）时返回 null，不抛 */
  private static nativeParseEdgeInsets(css: string): EdgeInsetsValue | null {
    try {
      if (typeof nativeModule.parseEdgeInsets !== 'function') {
        return null;
      }
      return nativeModule.parseEdgeInsets(css);
    } catch (_error) {
      return null;
    }
  }
}
