import { IAGenUILogger } from '@agenui/agenui';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { util } from '@kit.ArkTS';

/**
 * Custom RuntimeLogger implementation for AGenUI Playground
 * 
 * This logger forwards AGenUI engine logs to Harmony hilog with proper formatting.
 * It also provides a callback interface for displaying logs in the UI.
 */
export class PlaygroundRuntimeLogger implements IAGenUILogger {
  private static readonly TAG: string = 'AGenUI-Engine';
  private static readonly DOMAIN: number = 0x0000;
  
  /**
   * Callback interface for receiving log messages in the UI
   */
  public logCallback?: (level: number, tag: string, func: string, line: number, message: string) => void;
  
  constructor(callback?: (level: number, tag: string, func: string, line: number, message: string) => void) {
    this.logCallback = callback;
  }
  
  /**
   * Set the log callback for UI display
   */
  public setLogCallback(callback: (level: number, tag: string, func: string, line: number, message: string) => void): void {
    this.logCallback = callback;
  }
  
  onLog(level: number, tag: string, func: string, line: number, message: string): void {
    // Get current timestamp
    const timestamp = this.getTimestamp();
    
    // Format the log message with timestamp
    const formattedMessage = `${timestamp} [${func}@${line}] ${message}`;
    const logTag = tag && tag.length > 0 ? tag : PlaygroundRuntimeLogger.TAG;
    
    // Log to Harmony hilog based on level
    switch (level) {
      case 0: // DEBUG
        hilog.debug(PlaygroundRuntimeLogger.DOMAIN, logTag, '%{public}s', formattedMessage);
        break;
      case 1: // INFO
        hilog.info(PlaygroundRuntimeLogger.DOMAIN, logTag, '%{public}s', formattedMessage);
        break;
      case 2: // WARN
        hilog.warn(PlaygroundRuntimeLogger.DOMAIN, logTag, '%{public}s', formattedMessage);
        break;
      case 3: // ERROR
        hilog.error(PlaygroundRuntimeLogger.DOMAIN, logTag, '%{public}s', formattedMessage);
        break;
      case 4: // FATAL
        hilog.fatal(PlaygroundRuntimeLogger.DOMAIN, logTag, '%{public}s', formattedMessage);
        break;
      case 5: // PERFORMANCE
        hilog.debug(PlaygroundRuntimeLogger.DOMAIN, logTag, '[PERF] %{public}s', formattedMessage);
        break;
      default:
        hilog.debug(PlaygroundRuntimeLogger.DOMAIN, logTag, '%{public}s', formattedMessage);
        break;
    }
    
    // Notify UI callback if registered
    if (this.logCallback) {
      this.logCallback(level, tag, func, line, message);
    }
  }
  
  /**
   * Get human-readable log level name
   */
  public static getLevelName(level: number): string {
    switch (level) {
      case 0: return 'DEBUG';
      case 1: return 'INFO';
      case 2: return 'WARN';
      case 3: return 'ERROR';
      case 4: return 'FATAL';
      case 5: return 'PERF';
      default: return 'UNKNOWN';
    }
  }
  
  /**
   * Get formatted timestamp for log messages
   * Format: HH:mm:ss.SSS (e.g., 14:30:25.123)
   */
  private getTimestamp(): string {
    const date = new Date();
    const hours = this.padZero(date.getHours(), 2);
    const minutes = this.padZero(date.getMinutes(), 2);
    const seconds = this.padZero(date.getSeconds(), 2);
    const milliseconds = this.padZero(date.getMilliseconds(), 3);
    return `${hours}:${minutes}:${seconds}.${milliseconds}`;
  }
  
  /**
   * Pad number with leading zeros
   */
  private padZero(num: number, length: number): string {
    let str = num.toString();
    while (str.length < length) {
      str = '0' + str;
    }
    return str;
  }
}
