/**
 * SkillTest.ets
 *
 * AGenUI Harmony FunctionCall integration tests
 *
 * Test cases:
 *   SKILL-01  Register function via AGenUI.registerFunction
 *   SKILL-02  IFunctionCall.execute returns success result
 *   SKILL-03  IFunctionCall.execute with invalid input returns error
 *   SKILL-04  Unregister function via AGenUI.unregisterFunction
 *   SKILL-05  FunctionResult.createSuccess / createError factory methods
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { AGenUI, FunctionResult, FunctionConfig } from '@agenui/agenui';
import type { IFunctionCall, FunctionCallContext } from '@agenui/agenui';

/** AddFunction result interface */
interface AddFunctionResult {
  sum: number;
}

/** Parsed params interface */
interface AddParams {
  a: number;
  b: number;
}

/**
 * Test IFunctionCall: adds two numbers and returns the result
 */
class AddFunction implements IFunctionCall {
  execute(_context: FunctionCallContext, paramsJson: string): FunctionResult {
    const params: AddParams = JSON.parse(paramsJson) as AddParams;
    const a: number = params.a ?? 0;
    const b: number = params.b ?? 0;
    const resultData: AddFunctionResult = { sum: a + b };
    return FunctionResult.createSuccess(resultData);
  }

  getConfig(): FunctionConfig {
    const config = new FunctionConfig();
    config.name = 'test_add';
    return config;
  }
}

/**
 * Test IFunctionCall: always returns error
 */
class AlwaysFailFunction implements IFunctionCall {
  execute(_context: FunctionCallContext, _paramsJson: string): FunctionResult {
    return FunctionResult.createError('always fail');
  }

  getConfig(): FunctionConfig {
    const config = new FunctionConfig();
    config.name = 'test_fail';
    return config;
  }
}

export default function skillTest() {
  describe('SkillTest', () => {

    beforeEach(() => {
      // Clean up potentially leftover test functions
      AGenUI.unregisterFunction('test_add');
      AGenUI.unregisterFunction('test_fail');
    });

    afterEach(() => {
      AGenUI.unregisterFunction('test_add');
      AGenUI.unregisterFunction('test_fail');
    });

    /**
     * SKILL-01: Register function succeeds without exception
     */
    it('SKILL-01_register_function_success', 0, () => {
      const func = new AddFunction();
      let threw = false;
      try {
        AGenUI.registerFunction(func);
      } catch (e) {
        threw = true;
      }
      expect(threw).assertFalse();
    });

    /**
     * SKILL-02: IFunctionCall.execute returns success result with correct data
     */
    it('SKILL-02_execute_function_returns_success', 0, () => {
      const func = new AddFunction();
      const mockContext: FunctionCallContext = { instanceId: 1, surfaceId: 'test-surface' };
      const result: FunctionResult = func.execute(mockContext, '{"a": 3, "b": 7}');

      expect(result.result).assertTrue();
      const value: AddFunctionResult = result.value as AddFunctionResult;
      expect(value).not().assertUndefined();
      expect(value.sum).assertEqual(10);
    });

    /**
     * SKILL-03: IFunctionCall.execute with error function returns error result
     */
    it('SKILL-03_execute_function_returns_error', 0, () => {
      const func = new AlwaysFailFunction();
      const mockContext: FunctionCallContext = { instanceId: 1, surfaceId: 'test-surface' };
      const result: FunctionResult = func.execute(mockContext, '{}');

      expect(result.result).assertFalse();
      const errorMsg: string = result.value as string;
      expect(errorMsg).assertContain('always fail');
    });

    /**
     * SKILL-04: Unregister function does not throw
     */
    it('SKILL-04_unregister_function_success', 0, () => {
      AGenUI.registerFunction(new AddFunction());

      let threw = false;
      try {
        AGenUI.unregisterFunction('test_add');
      } catch (e) {
        threw = true;
      }
      expect(threw).assertFalse();
    });

    /**
     * SKILL-05: FunctionResult factory methods produce correct state
     */
    it('SKILL-05_function_result_factory_methods', 0, () => {
      const successData: AddFunctionResult = { sum: 42 };
      const successResult: FunctionResult = FunctionResult.createSuccess(successData);
      expect(successResult.result).assertTrue();
      const successValue: AddFunctionResult = successResult.value as AddFunctionResult;
      expect(successValue.sum).assertEqual(42);

      const errorResult: FunctionResult = FunctionResult.createError('test error');
      expect(errorResult.result).assertFalse();
      expect(errorResult.value as string).assertContain('test error');
    });
  });
}
