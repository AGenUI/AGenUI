import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { fileIo } from '@kit.CoreFileKit';
import { StabilityLogger } from './StabilityLogger';
import { CrashTracker } from './CrashTracker';
import { StabilityScenarioEngine, STRESS_SCENARIOS } from './StabilityScenarioEngine';

const TAG = 'StabilityTest';
const DOMAIN = 0x0000;

/**
 * StabilityTestAbility — headless Ability for running stability stress tests.
 * Launched by scripts/stability/harmony/launch.sh with parameters via Want extras.
 */
export default class StabilityTestAbility extends UIAbility {
  private scenario: string = 'all_combined';
  private durationMin: number = 480;
  private maxRounds: number = 0;
  private intervalMs: number = 100;
  private crashThreshold: number = 5;

  private logger!: StabilityLogger;
  private crashTracker!: CrashTracker;
  private engine!: StabilityScenarioEngine;
  private isRunning: boolean = false;
  private round: number = 0;
  private outputDir: string = '';
  private timerId: number = -1;

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG, 'StabilityTestAbility onCreate');

    // Parse parameters from Want
    const params = want.parameters;
    if (params) {
      this.scenario = (params['scenario'] as string) || 'all_combined';
      this.durationMin = (params['duration_minutes'] as number) || 480;
      this.maxRounds = (params['rounds'] as number) || 0;
      this.intervalMs = (params['interval_ms'] as number) || 100;
      this.crashThreshold = (params['crash_threshold'] as number) || 5;
    }

    // Setup output directory
    this.outputDir = `${this.context.filesDir}/stability`;
    try {
      if (!fileIo.accessSync(this.outputDir)) {
        fileIo.mkdirSync(this.outputDir, true);
      }
    } catch (e) {
      fileIo.mkdirSync(this.outputDir, true);
    }

    // Initialize components
    this.logger = new StabilityLogger(this.outputDir);
    this.crashTracker = new CrashTracker(this.outputDir, this.crashThreshold);
    this.engine = new StabilityScenarioEngine(this.context);

    this.logger.logEvent('start',
      `scenario=${this.scenario},duration_min=${this.durationMin},max_rounds=${this.maxRounds},interval_ms=${this.intervalMs}`);

    hilog.info(DOMAIN, TAG, 'Config: scenario=%{public}s, duration=%{public}dmin, interval=%{public}dms',
      this.scenario, this.durationMin, this.intervalMs);

    // Initialize AppStorage for overlay display (must be before page load)
    AppStorage.setOrCreate('stability_scenario', this.scenario);
    AppStorage.setOrCreate('stability_round', 0);
    AppStorage.setOrCreate('stability_maxRounds', this.maxRounds);
    AppStorage.setOrCreate('stability_elapsedMs', 0);
    AppStorage.setOrCreate('stability_errors', 0);
    AppStorage.setOrCreate('stability_isRunning', false);
    AppStorage.setOrCreate('stability_status', 'INITIALIZING');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('stability/StabilityTestPage');
    this.startTest();
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'StabilityTestAbility onDestroy');
    this.stopTest('onDestroy');
  }

  private startTest(): void {
    this.isRunning = true;
    this.round = 0;
    const startTime = Date.now();
    const durationMs = this.durationMin * 60 * 1000;

    // Mark running state in AppStorage
    AppStorage.set('stability_isRunning', true);
    AppStorage.set('stability_status', 'RUNNING');

    const runNextRound = (): void => {
      if (!this.isRunning) return;

      // Check duration limit
      if (Date.now() - startTime >= durationMs) {
        this.stopTest('duration_limit');
        return;
      }
      // Check rounds limit
      if (this.maxRounds > 0 && this.round >= this.maxRounds) {
        this.stopTest('rounds_limit');
        return;
      }

      // Select scenario
      const selectedScenario = this.selectScenario();
      if (!selectedScenario) {
        this.stopTest('all_blacklisted');
        return;
      }

      // Write crash state for attribution
      this.crashTracker.writeCrashState(selectedScenario);

      // Execute round
      this.round++;
      const roundStart = Date.now();
      let fixture: string | null = null;
      let error: string | null = null;
      let status = 'ok';

      try {
        fixture = this.engine.executeRound(selectedScenario);
      } catch (e) {
        error = String(e);
        status = 'error';
        this.crashTracker.recordCrash(selectedScenario);
        this.crashTracker.writeLastCrashScenario(selectedScenario);
      }

      const durationRound = Date.now() - roundStart;
      this.logger.logRound(this.round, selectedScenario, durationRound, status,
        fixture || undefined, error || undefined);

      // Update AppStorage metrics for overlay
      AppStorage.set('stability_scenario', selectedScenario);
      AppStorage.set('stability_round', this.round);
      AppStorage.set('stability_elapsedMs', Date.now() - startTime);
      if (status === 'error') {
        const currentErrors: number = AppStorage.get('stability_errors') || 0;
        AppStorage.set('stability_errors', currentErrors + 1);
      }

      // Schedule next
      const tid: number = setTimeout(runNextRound, this.intervalMs);
      this.timerId = tid;
    };

    // Start first round
    const firstTid: number = setTimeout(runNextRound, 0);
    this.timerId = firstTid;
  }

  private stopTest(reason: string): void {
    this.isRunning = false;
    if (this.timerId >= 0) {
      clearTimeout(this.timerId);
      this.timerId = -1;
    }
    this.crashTracker.markCleanExit();
    this.logger.logEvent('stop', reason);
    this.logger.close();

    // Update overlay status
    AppStorage.set('stability_isRunning', false);
    AppStorage.set('stability_status', `DONE(${reason})`);

    // Write done marker
    this.writeDoneMarker(reason);

    hilog.info(DOMAIN, TAG, 'Test stopped: %{public}s after %{public}d rounds', reason, this.round);

    // Terminate ability after a short delay
    setTimeout(() => {
      this.context.terminateSelf();
    }, 500);
  }

  private writeDoneMarker(reason: string): void {
    try {
      const donePath = `${this.outputDir}/stability_done.txt`;
      const content = `${reason}\n${Date.now()}\n`;
      const fd = fileIo.openSync(donePath,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC).fd;
      fileIo.writeSync(fd, content);
      fileIo.fsyncSync(fd);
      fileIo.closeSync(fd);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'Failed to write done marker: %{public}s', String(e));
    }
  }

  private selectScenario(): string | null {
    const scenarioUpper = this.scenario.toUpperCase();

    if (scenarioUpper === 'ALL_COMBINED' || scenarioUpper === 'ALL_STRESS') {
      const available = this.crashTracker.getAvailableScenarios(STRESS_SCENARIOS);
      if (available.length === 0) return null;
      return this.engine.pickRandom(available);
    }

    // Single scenario
    const single = scenarioUpper;
    if (this.crashTracker.isBlacklisted(single)) return null;
    return single;
  }
}
