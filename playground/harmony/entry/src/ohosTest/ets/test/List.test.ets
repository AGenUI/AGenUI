import abilityTest from './Ability.test';
import initializationTest from './tests/InitializationTest';
import surfaceLifecycleTest from './tests/SurfaceLifecycleTest';
import multiSurfaceTest from './tests/MultiSurfaceTest';
import componentRenderTest from './tests/ComponentRenderTest';
import skillTest from './tests/SkillTest';
import streamTest from './tests/StreamTest';
import functionCallTest from './tests/FunctionCallTest';

export default function testsuite() {
  abilityTest();
  initializationTest();
  surfaceLifecycleTest();
  multiSurfaceTest();
  componentRenderTest();
  skillTest();
  streamTest();
  functionCallTest();
}