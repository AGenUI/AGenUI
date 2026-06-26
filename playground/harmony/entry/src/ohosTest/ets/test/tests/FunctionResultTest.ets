import { describe, it, expect } from '@ohos/hypium';
import { FunctionResult, FunctionConfig } from '@agenui/agenui';

class TestValueObject {
  key: string = '';
  constructor(key: string) {
    this.key = key;
  }
}

class TestNestedObject {
  name: string = '';
  count: number = 0;
  constructor(name: string, count: number) {
    this.name = name;
    this.count = count;
  }
}

export default function functionResultTest() {
  describe('FunctionResult_createSuccess', () => {

    // ====== Normal Path ======

    it('createSuccess_withObject_setsResultTrue', 0, () => {
      const value = new TestValueObject('test');
      const result = FunctionResult.createSuccess(value);
      expect(result.result).assertTrue();
    });

    it('createSuccess_withObject_setsValue', 0, () => {
      const value = new TestValueObject('hello');
      const result = FunctionResult.createSuccess(value);
      expect(result.value).assertEqual(value);
    });

    it('createSuccess_withDifferentObject_setsResultTrue', 0, () => {
      const value = new TestNestedObject('deep', 42);
      const result = FunctionResult.createSuccess(value);
      expect(result.result).assertTrue();
    });

    it('createSuccess_withNestedObject_setsValue', 0, () => {
      const value = new TestNestedObject('nested', 100);
      const result = FunctionResult.createSuccess(value);
      expect(result.result).assertTrue();
      expect(result.value).assertEqual(value);
    });

    it('createSuccess_withArray_setsValue', 0, () => {
      const value = new Array<number>(1, 2, 3);
      const result = FunctionResult.createSuccess(value);
      expect(result.result).assertTrue();
      expect(result.value).assertEqual(value);
    });
  });

  describe('FunctionResult_createError', () => {

    // ====== Normal Path ======

    it('createError_withMessage_setsResultFalse', 0, () => {
      const result = FunctionResult.createError('something went wrong');
      expect(result.result).assertFalse();
    });

    it('createError_withMessage_setsValueToErrorString', 0, () => {
      const error = 'network timeout';
      const result = FunctionResult.createError(error);
      expect(result.value).assertEqual(error);
    });

    it('createError_withEmptyString_setsResultFalse', 0, () => {
      const result = FunctionResult.createError('');
      expect(result.result).assertFalse();
      expect(result.value).assertEqual('');
    });

    // ====== Boundary Values ======

    it('createError_withLongMessage_preservesFullMessage', 0, () => {
      const longError = 'A'.repeat(1000);
      const result = FunctionResult.createError(longError);
      expect(result.value).assertEqual(longError);
    });

    it('createError_withSpecialChars_preservesMessage', 0, () => {
      const error = 'Error: JSON parse failed';
      const result = FunctionResult.createError(error);
      expect(result.value).assertEqual(error);
    });
  });

  describe('FunctionResult_defaults', () => {

    it('newInstance_resultDefaultsFalse', 0, () => {
      const result = new FunctionResult();
      expect(result.result).assertFalse();
    });

    it('newInstance_valueDefaultsUndefined', 0, () => {
      const result = new FunctionResult();
      expect(result.value).assertUndefined();
    });
  });

  describe('FunctionConfig_defaults', () => {

    it('newInstance_nameDefaultsEmpty', 0, () => {
      const config = new FunctionConfig();
      expect(config.name).assertEqual('');
    });

    it('nameCanBeSet_returnsSetValue', 0, () => {
      const config = new FunctionConfig();
      config.name = 'testFunction';
      expect(config.name).assertEqual('testFunction');
    });

    it('nameCanBeSetToEmpty_returnsEmpty', 0, () => {
      const config = new FunctionConfig();
      config.name = 'something';
      config.name = '';
      expect(config.name).assertEqual('');
    });
  });
}
