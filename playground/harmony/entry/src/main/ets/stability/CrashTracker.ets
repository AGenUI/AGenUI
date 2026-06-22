import { fileIo } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'CrashTracker';
const DOMAIN = 0x0000;

/**
 * Tracks crash counts per scenario and implements blacklist logic.
 * Persists state to crash_registry.json for cross-restart survival.
 */
export class CrashTracker {
  private crashCounts: Map<string, number> = new Map();
  private threshold: number;
  private registryPath: string;
  private statePath: string;
  private lastCrashPath: string;

  constructor(outputDir: string, threshold: number = 5) {
    this.threshold = threshold;
    this.registryPath = `${outputDir}/crash_registry.json`;
    this.statePath = `${outputDir}/crash_state.json`;
    this.lastCrashPath = `${outputDir}/last_crash_scenario.txt`;
    this.loadRegistry();
  }

  /** Record a crash for the given scenario */
  recordCrash(scenario: string): void {
    const count = (this.crashCounts.get(scenario) || 0) + 1;
    this.crashCounts.set(scenario, count);
    this.saveRegistry();
    hilog.warn(DOMAIN, TAG, 'Crash recorded: %{public}s (count: %{public}d/%{public}d)',
      scenario, count, this.threshold);
  }

  /** Check if a scenario is blacklisted */
  isBlacklisted(scenario: string): boolean {
    return (this.crashCounts.get(scenario) || 0) >= this.threshold;
  }

  /** Get available (non-blacklisted) scenarios from a list */
  getAvailableScenarios(scenarios: string[]): string[] {
    return scenarios.filter(s => !this.isBlacklisted(s));
  }

  /** Write crash state before each round (for attribution on crash) */
  writeCrashState(scenario: string, subScenario?: string): void {
    let content = `{"scenario":"${scenario}","ts":"${new Date().toISOString()}"`;
    if (subScenario) {
      content += `,"sub_scenario":"${subScenario}"`;
    }
    content += '}';
    try {
      const fd = fileIo.openSync(this.statePath,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC).fd;
      fileIo.writeSync(fd, content);
      fileIo.fsyncSync(fd);
      fileIo.closeSync(fd);
    } catch (e) {
      // ignore
    }
  }

  /** Write last crash scenario (durable, read by monitor after restart) */
  writeLastCrashScenario(scenario: string): void {
    try {
      const content = JSON.stringify({ scenario: scenario, ts: new Date().toISOString() });
      const fd = fileIo.openSync(this.lastCrashPath,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC).fd;
      fileIo.writeSync(fd, content);
      fileIo.fsyncSync(fd);
      fileIo.closeSync(fd);
    } catch (e) {
      // ignore
    }
  }

  /** Mark clean exit */
  markCleanExit(): void {
    // Remove crash state file to indicate no crash occurred
    try {
      fileIo.unlinkSync(this.statePath);
    } catch (e) {
      // ignore
    }
  }

  private loadRegistry(): void {
    try {
      if (fileIo.accessSync(this.registryPath)) {
        const fd = fileIo.openSync(this.registryPath, fileIo.OpenMode.READ_ONLY).fd;
        const stat = fileIo.statSync(this.registryPath);
        const buf = new ArrayBuffer(stat.size);
        fileIo.readSync(fd, buf);
        fileIo.closeSync(fd);
        const text = String.fromCharCode(...new Uint8Array(buf));
        const data = JSON.parse(text) as Record<string, number>;
        for (const key of Object.keys(data)) {
          this.crashCounts.set(key, data[key]);
        }
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'Failed to load crash registry: %{public}s', String(e));
    }
  }

  private saveRegistry(): void {
    try {
      const obj: Record<string, number> = {};
      this.crashCounts.forEach((v, k) => {
        obj[k] = v;
      });
      const content = JSON.stringify(obj);
      const fd = fileIo.openSync(this.registryPath,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC).fd;
      fileIo.writeSync(fd, content);
      fileIo.fsyncSync(fd);
      fileIo.closeSync(fd);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'Failed to save crash registry: %{public}s', String(e));
    }
  }
}
