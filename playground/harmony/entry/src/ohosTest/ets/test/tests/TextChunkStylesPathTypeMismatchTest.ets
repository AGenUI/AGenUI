/**
 * TextChunkStylesPathTypeMismatchTest.ets
 *
 * RISK63: tryApplyTextChunk "styles" merge → parseDataBindingDataValue path
 *         type-mismatch → nlohmann::type_error → process killed
 *
 * NEW code path (commit 81a445fc, 2026-07-16, within last 15 days):
 *   fix #84070191 "Text流式效果中，对textChunk字段的进行解析处理时同时将styles更新"
 *
 * ComponentManager::tryApplyTextChunk() now merges inline "styles" from the
 * textChunk payload via DataValueParser::parseStylesDataValue -> parseDataValue
 * -> parseDataBindingDataValue, which does json["path"].get<std::string>() with
 * NO is_string() guard. A styles data-binding with a non-string "path" throws
 * nlohmann::type_error. updateComponents() only catches parse_error, so it
 * propagates uncaught -> std::terminate -> process killed.
 *
 * This differs from RISK58 (crashes on the "id" field). RISK63 uses a VALID
 * string id + textChunk and crashes inside the NEW styles-merge branch.
 *
 * Shared core/ code — cross-platform verification of the Android finding.
 *
 * IMPORTANT: This probe only exercises the real C++ engine when the ohosTest
 * native mock is DISABLED. Run it via
 * `tests/sdk-risk-probes/run_harmony_risk63_textchunk_styles_path.sh`, which
 * renames mock-config.json5 out of the way, builds against the real
 * liba2ui-capi.so, runs, and restores the mock. Under the default mock, the
 * native parser is stubbed and no crash occurs.
 *
 * Test cases:
 *   RISK63-01  styles.color.path = 12345 (integer) → crash
 *   RISK63-02  styles.fontSize.path = ["a","b"] (array) → crash
 *   RISK63-03  styles.color.path = null → crash
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager } from '@agenui/agenui';
import {
  sendAndWaitForSurface,
  sleep
} from '../base/AGenUIBaseTest';

export default function textChunkStylesPathTypeMismatchTest() {
  describe('TextChunkStylesPathTypeMismatchTest', () => {
    let sm: SurfaceManager;
    let context: common.UIAbilityContext;

    beforeEach(() => {
      context = getContext() as common.UIAbilityContext;
      sm = new SurfaceManager(context);
    });

    afterEach(() => {
      sm.destroy();
    });

    /**
     * RISK63-01: styles data-binding with "path" as integer.
     *
     * tryApplyTextChunk → parseStylesDataValue → parseDataValue →
     * parseDataBindingDataValue → json["path"].get<std::string>() on 12345
     * → nlohmann::type_error → process killed.
     */
    it('RISK63-01_textChunk_styles_path_integer_crash', 0, async () => {
      const surfaceId = 's_r63_hm_t1';

      const createJson = `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"test"}}`;
      await sendAndWaitForSurface(sm, createJson, surfaceId);

      // Setup: create a valid Text component so tryApplyTextChunk finds it.
      const setupJson = `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"txt1","component":"Text","attributes":{"text":"\\"hello\\""}}]}}`;
      sm.beginTextStream();
      sm.receiveTextChunk(setupJson);
      sm.endTextStream();
      await sleep(500);

      // Attack: valid string id + textChunk, styles.color.path is an integer.
      const attackJson = `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"txt1","component":"Text","textChunk":"world","styles":{"color":{"path":12345}}}]}}`;
      sm.beginTextStream();
      sm.receiveTextChunk(attackJson);
      sm.endTextStream();

      await sleep(2000);
      // If process survives, bug is not present or was fixed.
      expect(true).assertTrue();
    });

    /**
     * RISK63-02: styles data-binding with "path" as array.
     */
    it('RISK63-02_textChunk_styles_path_array_crash', 0, async () => {
      const surfaceId = 's_r63_hm_t2';

      const createJson = `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"test"}}`;
      await sendAndWaitForSurface(sm, createJson, surfaceId);

      const setupJson = `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"txt1","component":"Text","attributes":{"text":"\\"hello\\""}}]}}`;
      sm.beginTextStream();
      sm.receiveTextChunk(setupJson);
      sm.endTextStream();
      await sleep(500);

      const attackJson = `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"txt1","component":"Text","textChunk":"world","styles":{"fontSize":{"path":["a","b"]}}}]}}`;
      sm.beginTextStream();
      sm.receiveTextChunk(attackJson);
      sm.endTextStream();

      await sleep(2000);
      expect(true).assertTrue();
    });

    /**
     * RISK63-03: styles data-binding with "path" as null.
     */
    it('RISK63-03_textChunk_styles_path_null_crash', 0, async () => {
      const surfaceId = 's_r63_hm_t3';

      const createJson = `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"test"}}`;
      await sendAndWaitForSurface(sm, createJson, surfaceId);

      const setupJson = `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"txt1","component":"Text","attributes":{"text":"\\"hello\\""}}]}}`;
      sm.beginTextStream();
      sm.receiveTextChunk(setupJson);
      sm.endTextStream();
      await sleep(500);

      const attackJson = `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"txt1","component":"Text","textChunk":"world","styles":{"color":{"path":null}}}]}}`;
      sm.beginTextStream();
      sm.receiveTextChunk(attackJson);
      sm.endTextStream();

      await sleep(2000);
      expect(true).assertTrue();
    });
  });
}
