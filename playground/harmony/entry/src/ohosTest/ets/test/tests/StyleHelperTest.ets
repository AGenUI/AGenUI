import { describe, it, expect } from '@ohos/hypium';
import { StyleHelper } from '@agenui/agenui';

export default function styleHelperTest() {
  describe('StyleHelper_parseColor', () => {

    // ====== Normal Path ======

    it('parseColor_namedBlack_returns0xFF000000', 0, () => {
      const result = StyleHelper.parseColor('black');
      expect(result).assertEqual(0xFF000000);
    });

    it('parseColor_namedWhite_returns0xFFFFFFFF', 0, () => {
      const result = StyleHelper.parseColor('white');
      expect(result).assertEqual(0xFFFFFFFF);
    });

    it('parseColor_namedRed_returns0xFFFF0000', 0, () => {
      const result = StyleHelper.parseColor('red');
      expect(result).assertEqual(0xFFFF0000);
    });

    it('parseColor_namedGreen_returns0xFF00FF00', 0, () => {
      const result = StyleHelper.parseColor('green');
      expect(result).assertEqual(0xFF00FF00);
    });

    it('parseColor_namedBlue_returns0xFF0000FF', 0, () => {
      const result = StyleHelper.parseColor('blue');
      expect(result).assertEqual(0xFF0000FF);
    });

    it('parseColor_namedTransparent_returns0x00000000', 0, () => {
      const result = StyleHelper.parseColor('transparent');
      expect(result).assertEqual(0x00000000);
    });

    it('parseColor_namedGray_returns0xFF808080', 0, () => {
      const result = StyleHelper.parseColor('gray');
      expect(result).assertEqual(0xFF808080);
    });

    it('parseColor_namedGrey_returns0xFF808080', 0, () => {
      const result = StyleHelper.parseColor('grey');
      expect(result).assertEqual(0xFF808080);
    });

    it('parseColor_namedYellow_returns0xFFFFFF00', 0, () => {
      const result = StyleHelper.parseColor('yellow');
      expect(result).assertEqual(0xFFFFFF00);
    });

    it('parseColor_namedCaseInsensitive_returnsCorrect', 0, () => {
      const result = StyleHelper.parseColor('RED');
      expect(result).assertEqual(0xFFFF0000);
    });

    // ====== Hex6 Format ======

    it('parseColor_hex6Red_returns0xFFFF0000', 0, () => {
      const result = StyleHelper.parseColor('#FF0000');
      expect(result).assertEqual(0xFFFF0000);
    });

    it('parseColor_hex6Green_returns0xFF00FF00', 0, () => {
      const result = StyleHelper.parseColor('#00FF00');
      expect(result).assertEqual(0xFF00FF00);
    });

    it('parseColor_hex6Blue_returns0xFF0000FF', 0, () => {
      const result = StyleHelper.parseColor('#0000FF');
      expect(result).assertEqual(0xFF0000FF);
    });

    it('parseColor_hex6Black_returns0xFF000000', 0, () => {
      const result = StyleHelper.parseColor('#000000');
      expect(result).assertEqual(0xFF000000);
    });

    it('parseColor_hex6White_returns0xFFFFFFFF', 0, () => {
      const result = StyleHelper.parseColor('#FFFFFF');
      expect(result).assertEqual(0xFFFFFFFF);
    });

    it('parseColor_hex6Lowercase_returns0xFFAABBCC', 0, () => {
      const result = StyleHelper.parseColor('#aabbcc');
      expect(result).assertEqual(0xFFAABBCC);
    });

    // ====== Hex8 Format (RGBA → ARGB conversion) ======

    it('parseColor_hex8FullyOpaque_convertsCorrectly', 0, () => {
      // #FF0000FF → RGBA = R:FF G:00 B:00 A:FF → ARGB = 0xFFFF0000
      const result = StyleHelper.parseColor('#FF0000FF');
      expect(result).assertEqual(0xFFFF0000);
    });

    it('parseColor_hex8HalfAlpha_convertsCorrectly', 0, () => {
      // #00FF0080 → RGBA = R:00 G:FF B:00 A:80 → ARGB = 0x8000FF00
      const result = StyleHelper.parseColor('#00FF0080');
      expect(result).assertEqual(0x8000FF00);
    });

    it('parseColor_hex8FullyTransparent_convertsCorrectly', 0, () => {
      // #FFFFFF00 → RGBA = R:FF G:FF B:FF A:00 → ARGB = 0x00FFFFFF
      const result = StyleHelper.parseColor('#FFFFFF00');
      expect(result).assertEqual(0x00FFFFFF);
    });

    // ====== rgba() Format ======

    it('parseColor_rgba_parsesCorrectly', 0, () => {
      // rgba(255, 0, 0, 1) → A:FF R:FF G:00 B:00 → 0xFFFF0000
      const result = StyleHelper.parseColor('rgba(255, 0, 0, 1)');
      expect(result).assertEqual(0xFFFF0000);
    });

    it('parseColor_rgba_halfAlpha_parsesCorrectly', 0, () => {
      // rgba(0, 255, 0, 0.5) → A:128(0x80) R:00 G:FF B:00 → 0x8000FF00
      const result = StyleHelper.parseColor('rgba(0, 255, 0, 0.5)');
      expect(result).assertEqual(0x8000FF00);
    });

    it('parseColor_rgba_zeroAlpha_parsesCorrectly', 0, () => {
      // rgba(0, 0, 255, 0) → A:00 R:00 G:00 B:FF → 0x000000FF
      const result = StyleHelper.parseColor('rgba(0, 0, 255, 0)');
      expect(result).assertEqual(0x000000FF);
    });

    it('parseColor_rgb_noAlpha_assumesOpaque', 0, () => {
      // rgb(128, 64, 32) → A:FF R:80 G:40 B:20 → 0xFF804020
      const result = StyleHelper.parseColor('rgb(128, 64, 32)');
      expect(result).assertEqual(0xFF804020);
    });

    // ====== rgba() Boundary Values ======

    it('parseColor_rgba_clampOverflow_clamps', 0, () => {
      // Values over 255 should be clamped to 255
      const result = StyleHelper.parseColor('rgba(300, 300, 300, 1)');
      expect(result).assertEqual(0xFFFFFFFF);
    });

    it('parseColor_rgba_clampNegative_clamps', 0, () => {
      // Negative values should be clamped to 0
      const result = StyleHelper.parseColor('rgba(-10, -10, -10, 0)');
      expect(result).assertEqual(0x00000000);
    });

    // ====== Numeric Input ======

    it('parseColor_numericInput_returnsDirectly', 0, () => {
      const result = StyleHelper.parseColor(0xFF00FF00);
      expect(result).assertEqual(0xFF00FF00);
    });

    // ====== Error Path ======

    it('parseColor_emptyString_returnsTransparent', 0, () => {
      const result = StyleHelper.parseColor('');
      expect(result).assertEqual(0x00000000);
    });

    it('parseColor_whitespaceOnly_returnsTransparent', 0, () => {
      const result = StyleHelper.parseColor('   ');
      expect(result).assertEqual(0x00000000);
    });

    it('parseColor_invalidHex_returnsTransparent', 0, () => {
      const result = StyleHelper.parseColor('#GGG');
      expect(result).assertEqual(0x00000000);
    });

    it('parseColor_unknownNamed_returnsTransparent', 0, () => {
      const result = StyleHelper.parseColor('notacolor');
      expect(result).assertEqual(0x00000000);
    });

    it('parseColor_nonStringNonNumber_returnsTransparent', 0, () => {
      const result = StyleHelper.parseColor(true as Object);
      expect(result).assertEqual(0x00000000);
    });
  });

  describe('StyleHelper_parseDimension', () => {

    // ====== Normal Path ======

    it('parseDimension_numericInput_returnsDirectly', 0, () => {
      const result = StyleHelper.parseDimension(100);
      expect(result).assertEqual(100);
    });

    it('parseDimension_stringNumber_parsesCorrectly', 0, () => {
      const result = StyleHelper.parseDimension('200');
      expect(result).assertEqual(200);
    });

    it('parseDimension_pxSuffix_stripsAndParses', 0, () => {
      const result = StyleHelper.parseDimension('150px');
      expect(result).assertEqual(150);
    });

    it('parseDimension_decimalPx_parsesCorrectly', 0, () => {
      const result = StyleHelper.parseDimension('10.5px');
      expect(result).assertEqual(10.5);
    });

    it('parseDimension_auto_returnsNegative2', 0, () => {
      const result = StyleHelper.parseDimension('auto');
      expect(result).assertEqual(-2);
    });

    it('parseDimension_wrapContent_returnsNegative2', 0, () => {
      const result = StyleHelper.parseDimension('wrap_content');
      expect(result).assertEqual(-2);
    });

    it('parseDimension_matchParent_returnsNegative1', 0, () => {
      const result = StyleHelper.parseDimension('match_parent');
      expect(result).assertEqual(-1);
    });

    it('parseDimension_100Percent_returnsNegative1', 0, () => {
      const result = StyleHelper.parseDimension('100%');
      expect(result).assertEqual(-1);
    });

    // ====== Boundary Values ======

    it('parseDimension_zero_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension('0');
      expect(result).assertEqual(0);
    });

    it('parseDimension_zeroPx_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension('0px');
      expect(result).assertEqual(0);
    });

    it('parseDimension_emptyString_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension('');
      expect(result).assertEqual(0);
    });

    it('parseDimension_whitespaceOnly_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension('   ');
      expect(result).assertEqual(0);
    });

    it('parseDimension_caseInsensitive_auto_returnsNegative2', 0, () => {
      const result = StyleHelper.parseDimension('AUTO');
      expect(result).assertEqual(-2);
    });

    it('parseDimension_caseInsensitive_matchParent_returnsNegative1', 0, () => {
      const result = StyleHelper.parseDimension('MATCH_PARENT');
      expect(result).assertEqual(-1);
    });

    // ====== Error Path ======

    it('parseDimension_invalidString_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension('abc');
      expect(result).assertEqual(0);
    });

    it('parseDimension_nonStringNonNumber_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension(true as Object);
      expect(result).assertEqual(0);
    });

    it('parseDimension_pxWithNoNumber_returnsZero', 0, () => {
      const result = StyleHelper.parseDimension('px');
      expect(result).assertEqual(0);
    });
  });
}
