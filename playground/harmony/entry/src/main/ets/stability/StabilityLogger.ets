import { fileIo } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'StabilityLogger';
const DOMAIN = 0x0000;

/**
 * JSONL logger for stability test metrics.
 * Writes one JSON line per round and flushes to disk (updating mtime for heartbeat detection).
 */
export class StabilityLogger {
  private logFilePath: string;
  private fd: number = -1;

  constructor(outputDir: string) {
    try {
      if (!fileIo.accessSync(outputDir)) {
        fileIo.mkdirSync(outputDir, true);
      }
    } catch (e) {
      fileIo.mkdirSync(outputDir, true);
    }
    this.logFilePath = `${outputDir}/stability_log.jsonl`;
    // Append mode: launch.sh clears the file before first launch,
    // but crash-restarts by monitor.sh must preserve previous data.
    this.fd = fileIo.openSync(this.logFilePath,
      fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.APPEND).fd;
  }

  logRound(round: number, scenario: string, durationMs: number, status: string,
    fixture?: string, error?: string): void {
    let json = `{"ts":"${this.formatTimestamp()}","round":${round},"scenario":"${this.escapeJson(scenario)}","duration_ms":${durationMs},"memory_mb":${this.getMemoryMb()},"status":"${this.escapeJson(status)}"`;
    if (fixture) {
      json += `,"fixture":"${this.escapeJson(fixture)}"`;
    }
    if (error) {
      json += `,"error":"${this.escapeJson(error)}"`;
    }
    json += '}';
    this.writeLine(json);
  }

  logEvent(event: string, detail: string): void {
    const json = `{"ts":"${this.formatTimestamp()}","event":"${this.escapeJson(event)}","detail":"${this.escapeJson(detail)}"}`;
    this.writeLine(json);
  }

  close(): void {
    if (this.fd >= 0) {
      try {
        fileIo.closeSync(this.fd);
      } catch (e) {
        // ignore
      }
      this.fd = -1;
    }
  }

  getLogFilePath(): string {
    return this.logFilePath;
  }

  private writeLine(line: string): void {
    if (this.fd >= 0) {
      try {
        const data = line + '\n';
        fileIo.writeSync(this.fd, data);
        fileIo.fsyncSync(this.fd);
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'Write failed: %{public}s', String(e));
      }
    }
  }

  private formatTimestamp(): string {
    const d = new Date();
    const pad = (n: number, len: number = 2): string => String(n).padStart(len, '0');
    return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad(d.getMilliseconds(), 3)}`;
  }

  private getMemoryMb(): number {
    // HarmonyOS doesn't provide direct memory API in ArkTS; return 0 as placeholder
    return 0;
  }

  private escapeJson(s: string): string {
    return s.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n').replace(/\r/g, '\\r');
  }
}
