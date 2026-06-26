/**
 * ReentrantDeadlockTest.ets
 *
 * SDK Risk Probe: FunctionCallManager re-entrant deadlock (RISK22/RISK23)
 * Cross-platform verification for Harmony.
 *
 * Verified deadlock mechanism on Harmony (same-thread re-entrancy on main thread):
 *   1. Main thread processes data and calls getValueData() -> executeFunctionCallSync()
 *      which acquires _mutex.
 *   2. callSync() detects it is on main thread (pthread_equal) -> calls ArkTS
 *      callback directly on the same thread.
 *   3. Callback calls registerFunction()/unregisterFunction() -> tries to acquire
 *      _mutex again on the same thread -> DEADLOCK (std::mutex is non-recursive).
 *   4. System watchdog detects main thread unresponsive -> kills app (~19s).
 *
 * Note: The theoretical cross-thread AB-BA pattern (worker holds _mutex + TSFN
 * dispatch to main + cv.wait) also exists but the main-thread path deadlocks first.
 *
 * Detection: Test runner reports "App died" when system watchdog kills the app.
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { AGenUI, SurfaceManager, ISurfaceManagerListener, Surface, FunctionResult, FunctionConfig } from '@agenui/agenui';
import { hilog } from '@kit.PerformanceAnalysisKit';
import type { IFunctionCall, FunctionCallContext } from '@agenui/agenui';
import { sendAndWaitForRender, sleep } from '../base/AGenUIBaseTest';

const TAG = 'RiskProbe-Deadlock';
const DEADLOCK_CHECK_TIMEOUT_MS = 5000;

/** Typed result interface for FunctionResult.createSuccess() */
interface SimpleResultValue {
  result: string;
}

/** Callback type for function execution notification */
type ExecuteCallback = (count: number) => void;

/** Shared mutable counter for re-entrant functions */
let gReEntrantCallCount: number = 0;
let gReEntrantCallback: ExecuteCallback | null = null;
let gSelfUnregCallCount: number = 0;
let gSelfUnregCallback: ExecuteCallback | null = null;

/**
 * Check if the worker thread is still alive by trying to create a new surface.
 * Returns true if onCreateSurface fires within the timeout.
 */
function isWorkerThreadAlive(sm: SurfaceManager, surfaceId: string): Promise<boolean> {
  return new Promise<boolean>((resolve) => {
    let resolved = false;
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        sm.removeListener(listener);
        resolve(false);
      }
    }, DEADLOCK_CHECK_TIMEOUT_MS);

    const listener: ISurfaceManagerListener = {
      onCreateSurface: (surface: Surface) => {
        if (surface.surfaceId === surfaceId && !resolved) {
          resolved = true;
          clearTimeout(timer);
          sm.removeListener(listener);
          resolve(true);
        }
      }
    };
    sm.addListener(listener);

    const json = `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}`;
    sm.beginTextStream();
    sm.receiveTextChunk(json);
    sm.endTextStream();
  });
}

/**
 * Build createSurface JSON
 */
function buildCreateSurfaceJSON(surfaceId: string): string {
  return `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}`;
}

/**
 * Build updateComponents with a function call data binding on text property
 */
function buildUpdateWithFunctionCallBinding(surfaceId: string, funcName: string): string {
  return `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"root","component":"Column","children":["txt1"]},{"id":"txt1","component":"Text","text":{"call":"${funcName}","args":{"input":"test"}}}]}}`;
}

/**
 * ReEntrantRegisterFunction: calls AGenUI.registerFunction() inside execute()
 * to trigger re-entrant deadlock.
 */
class InnerFunction implements IFunctionCall {
  private innerName: string;
  constructor(name: string) {
    this.innerName = name;
  }
  execute(_context: FunctionCallContext, _paramsJson: string): FunctionResult {
    const rv: SimpleResultValue = { result: 'inner' };
    return FunctionResult.createSuccess(rv);
  }
  getConfig(): FunctionConfig {
    const cfg = new FunctionConfig();
    cfg.name = this.innerName;
    return cfg;
  }
}

class ReEntrantRegisterFunction implements IFunctionCall {
  execute(_context: FunctionCallContext, _paramsJson: string): FunctionResult {
    gReEntrantCallCount++;
    if (gReEntrantCallback !== null) {
      gReEntrantCallback(gReEntrantCallCount);
    }
    hilog.warn(0x0000, TAG, `reEntrantFuncHarmony called (count=${gReEntrantCallCount}), about to re-enter...`);
    try {
      const innerFunc = new InnerFunction(`innerFunc_${Date.now()}`);
      AGenUI.registerFunction(innerFunc);
      hilog.info(0x0000, TAG, 'reEntrantFuncHarmony: registerFunction returned (no deadlock)');
    } catch (e) {
      hilog.error(0x0000, TAG, `reEntrantFuncHarmony: registerFunction threw: ${e}`);
    }
    const rv: SimpleResultValue = { result: 'done' };
    return FunctionResult.createSuccess(rv);
  }
  getConfig(): FunctionConfig {
    const cfg = new FunctionConfig();
    cfg.name = 'reEntrantFuncHarmony';
    return cfg;
  }
}

class SelfUnregisterFunction implements IFunctionCall {
  execute(_context: FunctionCallContext, _paramsJson: string): FunctionResult {
    gSelfUnregCallCount++;
    if (gSelfUnregCallback !== null) {
      gSelfUnregCallback(gSelfUnregCallCount);
    }
    hilog.warn(0x0000, TAG, `selfUnregFuncHarmony called (count=${gSelfUnregCallCount}), unregistering self...`);
    try {
      AGenUI.unregisterFunction('selfUnregFuncHarmony');
      hilog.info(0x0000, TAG, 'selfUnregFuncHarmony: unregisterFunction returned (no deadlock)');
    } catch (e) {
      hilog.error(0x0000, TAG, `selfUnregFuncHarmony: unregisterFunction threw: ${e}`);
    }
    const rv: SimpleResultValue = { result: 'one-shot done' };
    return FunctionResult.createSuccess(rv);
  }
  getConfig(): FunctionConfig {
    const cfg = new FunctionConfig();
    cfg.name = 'selfUnregFuncHarmony';
    return cfg;
  }
}

export default function reentrantDeadlockTest() {
  describe('ReentrantDeadlockTest', () => {
    let sm: SurfaceManager;
    let context: common.UIAbilityContext;

    beforeEach(() => {
      context = getContext() as common.UIAbilityContext;
      sm = new SurfaceManager(context);
      gReEntrantCallCount = 0;
      gReEntrantCallback = null;
      gSelfUnregCallCount = 0;
      gSelfUnregCallback = null;
    });

    afterEach(() => {
      // sm.destroy() may hang if deadlocked — best effort
      try {
        sm.destroy();
      } catch (e) {
        hilog.warn(0x0000, TAG, `destroy failed (expected if deadlocked): ${e}`);
      }
    });

    /**
     * RISK22-Harmony: registerFunction re-entrant deadlock (cross-thread AB-BA)
     */
    it('RISK22_Harmony_reentrantDeadlockViaRegisterFunction', 0, async (done: Function) => {
      hilog.info(0x0000, TAG, '=== RISK22-Harmony: Re-entrant deadlock via registerFunction ===');
      let deadlockDetected = false;

      const reEntrantFunc = new ReEntrantRegisterFunction();
      AGenUI.registerFunction(reEntrantFunc);
      hilog.info(0x0000, TAG, 'Registered reEntrantFuncHarmony');

      // Step 1: Create surface with function call binding in a single stream.
      const surfaceId = 'deadlock-harmony-s1';
      const fullJson = buildCreateSurfaceJSON(surfaceId) +
        buildUpdateWithFunctionCallBinding(surfaceId, 'reEntrantFuncHarmony');

      hilog.info(0x0000, TAG, 'Streaming content with function call binding...');
      try {
        await sendAndWaitForRender(sm, fullJson, surfaceId);
        hilog.info(0x0000, TAG, 'sendAndWaitForRender returned (surface created)');
      } catch (e) {
        hilog.warn(0x0000, TAG, `sendAndWaitForRender threw: ${e}`);
      }

      // Step 2: Force re-evaluation of all function call data values.
      // On Harmony, getValueData() may not be called during initial component
      // creation. setDayNightMode triggers invalidateFunctionCallValues on the
      // worker thread, which marks all components dirty → flush → getValueData()
      // → executeFunctionCallSync → callSync → TSFN dispatch to main thread.
      hilog.info(0x0000, TAG, 'Calling setDayNightMode to trigger function call re-evaluation...');
      AGenUI.setDayNightMode('dark');

      // Wait for function call processing (may deadlock here)
      await sleep(3000);
      hilog.info(0x0000, TAG, `Function call count after render: ${gReEntrantCallCount}`);

      // Check if worker thread is alive
      const workerAlive = await isWorkerThreadAlive(sm, 'probe-harmony-alive-1');
      hilog.info(0x0000, TAG, `Worker thread alive: ${workerAlive}`);

      // On Harmony the deadlock is cross-thread AB-BA:
      //  - Worker: holds _mutex + cv.wait for main
      //  - Main: callback tries _mutex → blocked
      // If the main thread is blocked (registerFunction), we can't even
      // reach this line — the test itself hangs. So we also check for the
      // case where the function was never called (counter=0) but the worker
      // is frozen (data binding evaluation blocked the worker via TSFN+cv).
      if (!workerAlive) {
        deadlockDetected = true;
        hilog.error(0x0000, TAG, '*** HARMONY DEADLOCK DETECTED (RISK22: registerFunction) ***');
        hilog.error(0x0000, TAG, `Function called ${gReEntrantCallCount} time(s), worker thread frozen`);
      }

      hilog.info(0x0000, TAG, '=== RISK22-Harmony RESULT ===');
      hilog.info(0x0000, TAG, `Deadlock detected: ${deadlockDetected}`);
      if (deadlockDetected) {
        hilog.error(0x0000, TAG, 'ROOT CAUSE: Cross-thread deadlock (AB-BA pattern)');
        hilog.error(0x0000, TAG, '  Worker holds _mutex + waits for main thread (cv)');
        hilog.error(0x0000, TAG, '  Main thread callback calls registerFunction -> tries _mutex -> BLOCKED');
      }

      expect(deadlockDetected).assertTrue();
      done();
    });

    /**
     * RISK23-Harmony: unregisterFunction re-entrant deadlock (one-shot pattern)
     */
    it('RISK23_Harmony_reentrantDeadlockViaUnregisterFunction', 0, async (done: Function) => {
      hilog.info(0x0000, TAG, '=== RISK23-Harmony: Re-entrant deadlock via unregisterFunction ===');
      let deadlockDetected = false;

      const selfUnregFunc = new SelfUnregisterFunction();
      AGenUI.registerFunction(selfUnregFunc);
      hilog.info(0x0000, TAG, 'Registered selfUnregFuncHarmony (one-shot pattern)');

      // Stream content
      const surfaceId = 'deadlock-harmony-unreg-s1';
      const fullJson = buildCreateSurfaceJSON(surfaceId) +
        buildUpdateWithFunctionCallBinding(surfaceId, 'selfUnregFuncHarmony');

      hilog.info(0x0000, TAG, 'Streaming content with selfUnregFuncHarmony binding...');
      try {
        await sendAndWaitForRender(sm, fullJson, surfaceId);
        hilog.info(0x0000, TAG, 'sendAndWaitForRender returned (surface created)');
      } catch (e) {
        hilog.warn(0x0000, TAG, `sendAndWaitForRender threw: ${e}`);
      }

      // Force re-evaluation via setDayNightMode
      hilog.info(0x0000, TAG, 'Calling setDayNightMode to trigger function call re-evaluation...');
      AGenUI.setDayNightMode('dark');

      // Wait for function call processing (may deadlock here)
      await sleep(3000);
      hilog.info(0x0000, TAG, `Function call count: ${gSelfUnregCallCount}`);

      // Check worker thread liveness
      const workerAlive = await isWorkerThreadAlive(sm, 'probe-harmony-unreg-1');
      hilog.info(0x0000, TAG, `Worker thread alive: ${workerAlive}`);

      if (!workerAlive) {
        deadlockDetected = true;
        hilog.error(0x0000, TAG, '*** HARMONY DEADLOCK DETECTED (RISK23: unregisterFunction) ***');
      }

      hilog.info(0x0000, TAG, '=== RISK23-Harmony RESULT ===');
      hilog.info(0x0000, TAG, `Deadlock detected: ${deadlockDetected}`);
      if (deadlockDetected) {
        hilog.error(0x0000, TAG, 'One-shot pattern (unregister self) triggers same cross-thread deadlock');
      }

      expect(deadlockDetected).assertTrue();
      done();
    });
  });
}
