import localUnitTest from './LocalUnit.test';
import styleHelperTest from './StyleHelperTest.test';

export default function testsuite() {
  localUnitTest();
  styleHelperTest();
}