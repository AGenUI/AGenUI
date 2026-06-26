import { describe, it, expect } from '@ohos/hypium';
import { StyleHelper } from '@agenui/agenui';

/**
 * Unit tests for StyleHelper.parseColor / parseDimension.
 *
 * StyleHelper is a pure-function utility (no native bridge, no Looper),
 * therefore is suitable for local hypium unit testing.
 *
 * Source: platforms/harmony/agenui/src/main/ets/agenui/custom/StyleHelper.ets
 */
export default function styleHelperTest() {
  describe('StyleHelperTest', () => {

    // ============================================================
    // parseColor - number passthrough & non-string fallback
    // ============================================================
    it('parseColor_numberInput_returnsAsIs', 0, () => {
      expect(StyleHelper.parseColor(0xFFAABBCC)).assertEqual(0xFFAABBCC);
      expect(StyleHelper.parseColor(0)).assertEqual(0);
      expect(StyleHelper.parseColor(123456)).assertEqual(123456);
    });

    it('parseColor_nonStringNonNumber_returnsTransparent', 0, () => {
      // boolean / object / null-like Object inputs should fall to 0
      expect(StyleHelper.parseColor(true as Object)).assertEqual(0x00000000);
      expect(StyleHelper.parseColor(new Date() as Object)).assertEqual(0x00000000);
      expect(StyleHelper.parseColor([] as Object)).assertEqual(0x00000000);
    });

    // ============================================================
    // parseColor - empty / whitespace
    // ============================================================
    it('parseColor_emptyString_returnsTransparent', 0, () => {
      expect(StyleHelper.parseColor('')).assertEqual(0x00000000);
    });

    it('parseColor_whitespaceOnly_returnsTransparent', 0, () => {
      expect(StyleHelper.parseColor('   ')).assertEqual(0x00000000);
      expect(StyleHelper.parseColor('\t\n')).assertEqual(0x00000000);
    });

    // ============================================================
    // parseColor - named colors
    // ============================================================
    it('parseColor_namedColor_transparent', 0, () => {
      expect(StyleHelper.parseColor('transparent')).assertEqual(0x00000000);
    });

    it('parseColor_namedColor_black', 0, () => {
      expect(StyleHelper.parseColor('black')).assertEqual(0xFF000000);
    });

    it('parseColor_namedColor_white', 0, () => {
      expect(StyleHelper.parseColor('white')).assertEqual(0xFFFFFFFF);
    });

    it('parseColor_namedColor_red', 0, () => {
      expect(StyleHelper.parseColor('red')).assertEqual(0xFFFF0000);
    });

    it('parseColor_namedColor_green', 0, () => {
      expect(StyleHelper.parseColor('green')).assertEqual(0xFF00FF00);
    });

    it('parseColor_namedColor_blue', 0, () => {
      expect(StyleHelper.parseColor('blue')).assertEqual(0xFF0000FF);
    });

    it('parseColor_namedColor_yellow', 0, () => {
      expect(StyleHelper.parseColor('yellow')).assertEqual(0xFFFFFF00);
    });

    it('parseColor_namedColor_gray_and_grey_areEquivalent', 0, () => {
      expect(StyleHelper.parseColor('gray')).assertEqual(0xFF808080);
      expect(StyleHelper.parseColor('grey')).assertEqual(0xFF808080);
    });

    it('parseColor_namedColor_caseInsensitive', 0, () => {
      expect(StyleHelper.parseColor('BLACK')).assertEqual(0xFF000000);
      expect(StyleHelper.parseColor('White')).assertEqual(0xFFFFFFFF);
      expect(StyleHelper.parseColor('rEd')).assertEqual(0xFFFF0000);
    });

    it('parseColor_namedColor_trimsWhitespace', 0, () => {
      expect(StyleHelper.parseColor('  black  ')).assertEqual(0xFF000000);
      expect(StyleHelper.parseColor('\twhite\n')).assertEqual(0xFFFFFFFF);
    });

    // ============================================================
    // parseColor - hex format
    // ============================================================
    it('parseColor_hex6_addsOpaqueAlpha', 0, () => {
      // #RRGGBB -> 0xFFRRGGBB
      expect(StyleHelper.parseColor('#FF0000')).assertEqual(0xFFFF0000);
      expect(StyleHelper.parseColor('#00FF00')).assertEqual(0xFF00FF00);
      expect(StyleHelper.parseColor('#0000FF')).assertEqual(0xFF0000FF);
      expect(StyleHelper.parseColor('#123456')).assertEqual(0xFF123456);
    });

    it('parseColor_hex6_lowercase_alsoWorks', 0, () => {
      // hex string carries no case requirement before parseInt
      expect(StyleHelper.parseColor('#abcdef')).assertEqual(0xFFABCDEF);
    });

    it('parseColor_hex8_movesAlphaToFront', 0, () => {
      // #RRGGBBAA -> 0xAARRGGBB (per source code)
      expect(StyleHelper.parseColor('#FF000080')).assertEqual(0x80FF0000);
      expect(StyleHelper.parseColor('#11223344')).assertEqual(0x44112233);
      expect(StyleHelper.parseColor('#000000FF')).assertEqual(0xFF000000);
    });

    it('parseColor_hexInvalidLength_returnsTransparent', 0, () => {
      // Only length 6 and 8 are accepted; others fall through to NaN paths.
      // #FF -> length 2, no rgb match, returns 0
      expect(StyleHelper.parseColor('#FF')).assertEqual(0x00000000);
      // #FFFFF -> length 5, no rgb match, returns 0
      expect(StyleHelper.parseColor('#FFFFF')).assertEqual(0x00000000);
      // #FFFFFFF -> length 7, returns 0
      expect(StyleHelper.parseColor('#FFFFFFF')).assertEqual(0x00000000);
    });

    // ============================================================
    // parseColor - rgb / rgba format
    // ============================================================
    it('parseColor_rgb_threeChannels', 0, () => {
      // rgb path uses bitwise OR (returns signed int32). Use `| 0` on expected to match.
      expect(StyleHelper.parseColor('rgb(255, 0, 0)')).assertEqual(0xFFFF0000 | 0);
      expect(StyleHelper.parseColor('rgb(0, 255, 0)')).assertEqual(0xFF00FF00 | 0);
      expect(StyleHelper.parseColor('rgb(0, 0, 255)')).assertEqual(0xFF0000FF | 0);
      expect(StyleHelper.parseColor('rgb(18, 52, 86)')).assertEqual(0xFF123456 | 0);
    });

    it('parseColor_rgba_fourChannels', 0, () => {
      expect(StyleHelper.parseColor('rgba(255, 0, 0, 1)')).assertEqual(0xFFFF0000 | 0);
      expect(StyleHelper.parseColor('rgba(0, 0, 0, 0)')).assertEqual(0x00000000);
      // alpha 0.5 -> round(127.5) = 128 = 0x80
      expect(StyleHelper.parseColor('rgba(255, 0, 0, 0.5)')).assertEqual(0x80FF0000 | 0);
    });

    it('parseColor_rgb_clampsOverflowChannels', 0, () => {
      // 999 -> clamp to 255
      expect(StyleHelper.parseColor('rgb(999, 0, 0)')).assertEqual(0xFFFF0000 | 0);
      // negative -> clamp to 0
      expect(StyleHelper.parseColor('rgb(-50, 0, 0)')).assertEqual(0xFF000000 | 0);
    });

    it('parseColor_rgba_clampsAlphaRange', 0, () => {
      // alpha > 1 -> clamp to 255
      expect(StyleHelper.parseColor('rgba(0, 0, 0, 5)')).assertEqual(0xFF000000 | 0);
      // alpha < 0 -> clamp to 0
      expect(StyleHelper.parseColor('rgba(255, 255, 255, -0.5)')).assertEqual(0x00FFFFFF);
    });

    it('parseColor_rgb_caseInsensitive', 0, () => {
      expect(StyleHelper.parseColor('RGB(255, 0, 0)')).assertEqual(0xFFFF0000 | 0);
      expect(StyleHelper.parseColor('Rgba(0, 0, 0, 1)')).assertEqual(0xFF000000 | 0);
    });

    it('parseColor_rgb_handlesExtraWhitespace', 0, () => {
      expect(StyleHelper.parseColor('rgb(  255 ,   0 ,  0 )')).assertEqual(0xFFFF0000 | 0);
    });

    it('parseColor_rgb_invalidPartsCount_returnsTransparent', 0, () => {
      // 2 parts -> not 3 or 4, returns 0
      expect(StyleHelper.parseColor('rgb(255, 0)')).assertEqual(0x00000000);
      // 5 parts -> not 3 or 4, returns 0
      expect(StyleHelper.parseColor('rgba(255, 0, 0, 1, 1)')).assertEqual(0x00000000);
    });

    it('parseColor_arbitraryString_returnsTransparent', 0, () => {
      expect(StyleHelper.parseColor('not-a-color')).assertEqual(0x00000000);
      expect(StyleHelper.parseColor('hsl(120, 100%, 50%)')).assertEqual(0x00000000);
      expect(StyleHelper.parseColor('null')).assertEqual(0x00000000);
    });

    // ============================================================
    // parseDimension - number passthrough & non-string fallback
    // ============================================================
    it('parseDimension_numberInput_returnsAsIs', 0, () => {
      expect(StyleHelper.parseDimension(0)).assertEqual(0);
      expect(StyleHelper.parseDimension(12)).assertEqual(12);
      expect(StyleHelper.parseDimension(12.5)).assertEqual(12.5);
      expect(StyleHelper.parseDimension(-5)).assertEqual(-5);
    });

    it('parseDimension_nonStringNonNumber_returnsZero', 0, () => {
      expect(StyleHelper.parseDimension(true as Object)).assertEqual(0);
      expect(StyleHelper.parseDimension(new Date() as Object)).assertEqual(0);
      expect(StyleHelper.parseDimension([] as Object)).assertEqual(0);
    });

    // ============================================================
    // parseDimension - empty / whitespace
    // ============================================================
    it('parseDimension_emptyString_returnsZero', 0, () => {
      expect(StyleHelper.parseDimension('')).assertEqual(0);
    });

    it('parseDimension_whitespaceOnly_returnsZero', 0, () => {
      expect(StyleHelper.parseDimension('   ')).assertEqual(0);
    });

    // ============================================================
    // parseDimension - special keywords
    // ============================================================
    it('parseDimension_auto_returnsMinusTwo', 0, () => {
      expect(StyleHelper.parseDimension('auto')).assertEqual(-2);
    });

    it('parseDimension_wrapContent_returnsMinusTwo', 0, () => {
      expect(StyleHelper.parseDimension('wrap_content')).assertEqual(-2);
    });

    it('parseDimension_matchParent_returnsMinusOne', 0, () => {
      expect(StyleHelper.parseDimension('match_parent')).assertEqual(-1);
    });

    it('parseDimension_hundredPercent_returnsMinusOne', 0, () => {
      expect(StyleHelper.parseDimension('100%')).assertEqual(-1);
    });

    it('parseDimension_keywords_caseInsensitive', 0, () => {
      // raw is lowercased before keyword check
      expect(StyleHelper.parseDimension('AUTO')).assertEqual(-2);
      expect(StyleHelper.parseDimension('Wrap_Content')).assertEqual(-2);
      expect(StyleHelper.parseDimension('MATCH_PARENT')).assertEqual(-1);
    });

    it('parseDimension_keywords_trimsWhitespace', 0, () => {
      expect(StyleHelper.parseDimension('  auto  ')).assertEqual(-2);
      expect(StyleHelper.parseDimension('\tmatch_parent\n')).assertEqual(-1);
    });

    // ============================================================
    // parseDimension - px units
    // ============================================================
    it('parseDimension_pxInteger_strippedAndParsed', 0, () => {
      expect(StyleHelper.parseDimension('12px')).assertEqual(12);
      expect(StyleHelper.parseDimension('100px')).assertEqual(100);
      expect(StyleHelper.parseDimension('0px')).assertEqual(0);
    });

    it('parseDimension_pxFloat_preservesDecimal', 0, () => {
      expect(StyleHelper.parseDimension('12.5px')).assertEqual(12.5);
      expect(StyleHelper.parseDimension('0.25px')).assertEqual(0.25);
    });

    it('parseDimension_pxNegative_supported', 0, () => {
      expect(StyleHelper.parseDimension('-8px')).assertEqual(-8);
    });

    it('parseDimension_pxCaseInsensitive', 0, () => {
      // raw is lowercased
      expect(StyleHelper.parseDimension('12PX')).assertEqual(12);
      expect(StyleHelper.parseDimension('12Px')).assertEqual(12);
    });

    it('parseDimension_pxInvalid_returnsZero', 0, () => {
      // "abcpx" -> parseFloat("abc") = NaN -> NaN || 0 -> 0
      expect(StyleHelper.parseDimension('abcpx')).assertEqual(0);
    });

    // ============================================================
    // parseDimension - bare numeric strings
    // ============================================================
    it('parseDimension_bareInteger_parsed', 0, () => {
      expect(StyleHelper.parseDimension('42')).assertEqual(42);
      expect(StyleHelper.parseDimension('0')).assertEqual(0);
    });

    it('parseDimension_bareFloat_parsed', 0, () => {
      expect(StyleHelper.parseDimension('3.14')).assertEqual(3.14);
    });

    it('parseDimension_bareNegative_parsed', 0, () => {
      expect(StyleHelper.parseDimension('-7')).assertEqual(-7);
    });

    it('parseDimension_arbitraryString_returnsZero', 0, () => {
      expect(StyleHelper.parseDimension('abc')).assertEqual(0);
      expect(StyleHelper.parseDimension('null')).assertEqual(0);
      expect(StyleHelper.parseDimension('undefined')).assertEqual(0);
    });

    // Numbers with non-px suffix: parseFloat reads leading digits, returns that.
    // (parseFloat('12em') === 12, so this is current contract.)
    it('parseDimension_nonPxSuffix_parsesLeadingNumber', 0, () => {
      expect(StyleHelper.parseDimension('12em')).assertEqual(12);
      expect(StyleHelper.parseDimension('5rem')).assertEqual(5);
    });
  });
}
