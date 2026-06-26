import abilityTest from './Ability.test';
import initializationTest from './tests/InitializationTest';
import surfaceLifecycleTest from './tests/SurfaceLifecycleTest';
import multiSurfaceTest from './tests/MultiSurfaceTest';
import componentRenderTest from './tests/ComponentRenderTest';
import skillTest from './tests/SkillTest';
import streamTest from './tests/StreamTest';
import functionCallTest from './tests/FunctionCallTest';
import styleHelperTest from './tests/StyleHelperTest';
import functionResultTest from './tests/FunctionResultTest';
import imageLoaderDataTest from './tests/ImageLoaderDataTest';
import reentrantDeadlockTest from './tests/ReentrantDeadlockTest';

export default function testsuite() {
  abilityTest();
  initializationTest();
  surfaceLifecycleTest();
  multiSurfaceTest();
  componentRenderTest();
  skillTest();
  streamTest();
  functionCallTest();
  styleHelperTest();
  functionResultTest();
  imageLoaderDataTest();
  reentrantDeadlockTest();
}