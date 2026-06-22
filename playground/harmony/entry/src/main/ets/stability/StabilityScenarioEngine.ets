import { SurfaceManager, ISurfaceManagerListener, Surface, AGenUI } from '@agenui/agenui';
import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'StabilityEngine';
const DOMAIN = 0x0000;

/** All stress scenarios */
export const STRESS_SCENARIOS: string[] = [
  'SESSION_STORM', 'STREAM_MARATHON', 'MULTI_SURFACE', 'ACTION_FLOOD',
  'THEME_SWITCH', 'INTERRUPT_RECOVER', 'EXTREME_RENDER', 'SDK_ROBUSTNESS'
];

/** Check if scenario is a stress type */
export function isStressScenario(name: string): boolean {
  return STRESS_SCENARIOS.includes(name);
}

/**
 * Executes stress scenarios against the AGenUI SDK (HarmonyOS).
 */
export class StabilityScenarioEngine {
  private context: common.UIAbilityContext;

  constructor(context: common.UIAbilityContext) {
    this.context = context;
  }

  /**
   * Execute one round of the specified stress scenario.
   * Returns fixture/result string or null.
   */
  executeRound(scenario: string): string | null {
    switch (scenario) {
      case 'SESSION_STORM':
        return this.executeSessionStorm();
      case 'STREAM_MARATHON':
        return this.executeStreamMarathon();
      case 'MULTI_SURFACE':
        return this.executeMultiSurface();
      case 'ACTION_FLOOD':
        return this.executeActionFlood();
      case 'THEME_SWITCH':
        return this.executeThemeSwitch();
      case 'INTERRUPT_RECOVER':
        return this.executeInterruptRecover();
      case 'EXTREME_RENDER':
        return this.executeExtremeRender();
      case 'SDK_ROBUSTNESS':
        return this.executeSdkRobustness();
      default:
        return null;
    }
  }

  /** Pick a random stress scenario */
  pickRandom(available: string[]): string {
    return available[Math.floor(Math.random() * available.length)];
  }

  /** Synchronous delay (busy-wait). Required because native SurfaceManager needs time to settle. */
  private sleepSync(ms: number): void {
    const end = Date.now() + ms;
    while (Date.now() < end) { /* busy wait */ }
  }

  // S1: Rapid create/destroy SurfaceManager instances
  private executeSessionStorm(): string | null {
    for (let i = 0; i < 10; i++) {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON(`storm-${i}`));
      sm.endTextStream();
      this.sleepSync(10);
      sm.destroy();
      this.sleepSync(10);
    }
    return null;
  }

  // S2: Single surface with long streaming (100 chunks)
  private executeStreamMarathon(): string | null {
    const sm = new SurfaceManager(this.context);
    sm.beginTextStream();
    sm.receiveTextChunk(this.buildCreateSurfaceJSON('marathon'));
    for (let i = 0; i < 100; i++) {
      const update = `{"version":"v0.9","updateDataModel":{"surfaceId":"marathon","value":{"counter":${i}}}}`;
      sm.receiveTextChunk(update);
    }
    sm.endTextStream();
    this.sleepSync(50);
    sm.destroy();
    return null;
  }

  // S3: Multiple surfaces active simultaneously
  private executeMultiSurface(): string | null {
    const sm = new SurfaceManager(this.context);
    sm.beginTextStream();
    for (let i = 0; i < 5; i++) {
      sm.receiveTextChunk(this.buildCreateSurfaceJSON(`multi-${i}`));
    }
    for (let j = 0; j < 20; j++) {
      const idx = Math.floor(Math.random() * 5);
      const update = `{"version":"v0.9","updateDataModel":{"surfaceId":"multi-${idx}","value":{"update":${j}}}}`;
      sm.receiveTextChunk(update);
    }
    for (let i = 0; i < 5; i++) {
      sm.receiveTextChunk(`{"version":"v0.9","deleteSurface":{"surfaceId":"multi-${i}"}}`);
    }
    sm.endTextStream();
    this.sleepSync(50);
    sm.destroy();
    return null;
  }

  // S4: Rapid data model sync
  private executeActionFlood(): string | null {
    const sm = new SurfaceManager(this.context);
    sm.beginTextStream();
    sm.receiveTextChunk(this.buildCreateSurfaceJSON('flood'));
    sm.endTextStream();
    this.sleepSync(50);
    for (let i = 0; i < 50; i++) {
      sm.syncState('flood', `comp-${i % 5}`, JSON.stringify({ value: `input_${i}` }));
    }
    this.sleepSync(50);
    sm.destroy();
    return null;
  }

  // S5: Day/night mode switching
  private executeThemeSwitch(): string | null {
    for (let i = 0; i < 20; i++) {
      AGenUI.setDayNightMode(i % 2 === 0 ? 'light' : 'dark');
      this.sleepSync(5);
    }
    return null;
  }

  // S6: Begin stream, interrupt (destroy), recreate
  private executeInterruptRecover(): string | null {
    let sm = new SurfaceManager(this.context);
    sm.beginTextStream();
    sm.receiveTextChunk(this.buildCreateSurfaceJSON('interrupt'));
    this.sleepSync(20);
    sm.destroy(); // interrupt mid-stream
    this.sleepSync(50);

    sm = new SurfaceManager(this.context);
    sm.beginTextStream();
    sm.receiveTextChunk(this.buildCreateSurfaceJSON('recover'));
    sm.endTextStream();
    this.sleepSync(50);
    sm.destroy();
    return null;
  }

  // S7: Extreme render (simple placeholder since no fixture system yet)
  private executeExtremeRender(): string | null {
    const sm = new SurfaceManager(this.context);
    sm.beginTextStream();
    // Create a surface with many components
    const components: string[] = [];
    for (let i = 0; i < 50; i++) {
      components.push(`{"id":"txt-${i}","component":"Text","text":"Item ${i} - ${'x'.repeat(100)}"}`);
    }
    const msg = `{"version":"v0.9","createSurface":{"surfaceId":"extreme","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}`;
    sm.receiveTextChunk(msg);
    const updateMsg = `{"version":"v0.9","updateComponents":{"surfaceId":"extreme","components":[{"id":"root","component":"Column","children":[${components.map((_, i) => `"txt-${i}"`).join(',')}],"align":"stretch"},${components.join(',')}]}}`;
    sm.receiveTextChunk(updateMsg);
    sm.endTextStream();
    this.sleepSync(100);
    sm.destroy();
    return 'inline_extreme_50_components';
  }

  // S8: SDK Robustness — 12 defensive sub-cases
  private executeSdkRobustness(): string {
    const results: string[] = [];
    let passed = 0;
    const total = 12;

    // R1: use_after_destroy
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r1'));
      sm.endTextStream();
      sm.destroy();
      const r1Exceptions: string[] = [];
      try { sm.beginTextStream(); } catch (t) { r1Exceptions.push(`begin:${(t as Error).name}`); }
      try { sm.receiveTextChunk('{"version":"v0.9"}'); } catch (t) { r1Exceptions.push(`receive:${(t as Error).name}`); }
      try { sm.endTextStream(); } catch (t) { r1Exceptions.push(`end:${(t as Error).name}`); }
      try { sm.destroy(); } catch (t) { r1Exceptions.push(`destroy:${(t as Error).name}`); }
      passed++;
      results.push(r1Exceptions.length === 0 ? 'R1:OK(no_throw)' : `R1:OK(${r1Exceptions.join(';')})`);
      if (r1Exceptions.length > 0) {
        hilog.warn(DOMAIN, TAG, 'R1 exceptions: %{public}s', r1Exceptions.join(';'));
      }
    } catch (e) {
      results.push(`R1:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R1 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R2: double_destroy
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r2'));
      sm.endTextStream();
      sm.destroy();
      sm.destroy();
      passed++;
      results.push('R2:OK');
    } catch (e) {
      results.push(`R2:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R2 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R3: receive_without_begin
    try {
      const sm = new SurfaceManager(this.context);
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r3'));
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push('R3:OK');
    } catch (e) {
      results.push(`R3:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R3 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R4: end_without_begin
    try {
      const sm = new SurfaceManager(this.context);
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push('R4:OK');
    } catch (e) {
      results.push(`R4:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R4 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R5: double_end_stream
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r5'));
      sm.endTextStream();
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push('R5:OK');
    } catch (e) {
      results.push(`R5:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R5 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R6: double_begin_stream
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r6'));
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push('R6:OK');
    } catch (e) {
      results.push(`R6:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R6 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R7: syncState on nonexistent surface
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r7'));
      sm.endTextStream();
      sm.syncState('nonexistent-xyz', 'comp1', '{"value":true}');
      sm.syncState('', 'comp1', '{"value":true}');
      sm.destroy();
      passed++;
      results.push('R7:OK');
    } catch (e) {
      results.push(`R7:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R7 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R8: null/undefined arguments
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      const r8Exceptions: string[] = [];
      const nullStr: string | null = null;
      const undefStr: string | undefined = undefined;
      try { sm.receiveTextChunk(undefStr!); } catch (t) { r8Exceptions.push(`receiveUndef:${(t as Error).name}`); }
      try { sm.receiveTextChunk(nullStr!); } catch (t) { r8Exceptions.push(`receiveNull:${(t as Error).name}`); }
      try { sm.syncState(nullStr!, nullStr!, nullStr!); } catch (t) { r8Exceptions.push(`syncNull:${(t as Error).name}`); }
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push(r8Exceptions.length === 0 ? 'R8:OK(no_throw)' : `R8:OK(${r8Exceptions.join(';')})`);
      if (r8Exceptions.length > 0) {
        hilog.warn(DOMAIN, TAG, 'R8 exceptions: %{public}s', r8Exceptions.join(';'));
      }
    } catch (e) {
      results.push(`R8:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R8 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R9: empty_arguments
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk('');
      sm.receiveTextChunk('   ');
      sm.syncState('', '', '');
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push('R9:OK');
    } catch (e) {
      results.push(`R9:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R9 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R10: malformed_json
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      const malformed = [
        '{broken json', '{{{{',
        '{"version":"v0.9","createSurface":{}}',
        '{"version":"v0.9","updateDataModel":{"surfaceId":null}}',
        '<html>not json</html>',
        'x'.repeat(10000),
        '{"version":"v99.99","unknownOp":{"surfaceId":"r10"}}',
        'null', '[]', '0'
      ];
      const r10Exceptions: string[] = [];
      for (let i = 0; i < malformed.length; i++) {
        try { sm.receiveTextChunk(malformed[i]); } catch (t) {
          r10Exceptions.push(`payload${i}:${(t as Error).name}`);
        }
      }
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push(r10Exceptions.length === 0 ? 'R10:OK(no_throw)' : `R10:OK(${r10Exceptions.join(';')})`);
      if (r10Exceptions.length > 0) {
        hilog.warn(DOMAIN, TAG, 'R10 exceptions: %{public}s', r10Exceptions.join(';'));
      }
    } catch (e) {
      results.push(`R10:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R10 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R11: remove_unregistered_listener
    try {
      const sm = new SurfaceManager(this.context);
      const listener: ISurfaceManagerListener = {};
      sm.removeListener(listener); // never added
      sm.removeListener(listener); // twice
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r11'));
      sm.endTextStream();
      sm.destroy();
      passed++;
      results.push('R11:OK');
    } catch (e) {
      results.push(`R11:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R11 crashed: %{public}s', String(e));
    }

    this.sleepSync(20);

    // R12: listener_after_destroy
    try {
      const sm = new SurfaceManager(this.context);
      sm.beginTextStream();
      sm.receiveTextChunk(this.buildCreateSurfaceJSON('r12'));
      sm.endTextStream();
      sm.destroy();
      const r12Exceptions: string[] = [];
      try {
        sm.addListener({ onCreateSurface: () => {}, onDeleteSurface: () => {} });
      } catch (t) { r12Exceptions.push(`addListener:${(t as Error).name}`); }
      try {
        sm.removeListener({ onCreateSurface: () => {}, onDeleteSurface: () => {} });
      } catch (t) { r12Exceptions.push(`removeListener:${(t as Error).name}`); }
      passed++;
      results.push(r12Exceptions.length === 0 ? 'R12:OK(no_throw)' : `R12:OK(${r12Exceptions.join(';')})`);
      if (r12Exceptions.length > 0) {
        hilog.warn(DOMAIN, TAG, 'R12 exceptions: %{public}s', r12Exceptions.join(';'));
      }
    } catch (e) {
      results.push(`R12:CRASH(${(e as Error).name})`);
      hilog.error(DOMAIN, TAG, 'R12 crashed: %{public}s', String(e));
    }

    const summary = `sdk_robustness[${results.join(',')}] ${passed}/${total} passed`;
    hilog.info(DOMAIN, TAG, '%{public}s', summary);
    return summary;
  }

  // Helper: build create surface JSON
  private buildCreateSurfaceJSON(surfaceId: string): string {
    return `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}`;
  }
}
