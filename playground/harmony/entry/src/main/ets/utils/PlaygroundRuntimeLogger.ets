import { IAGenUILogger } from '@agenui/agenui';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { common } from '@kit.AbilityKit';
import { fileIo as fs } from '@kit.CoreFileKit';

/**
 * Custom RuntimeLogger implementation for AGenUI Playground
 * 
 * This logger forwards AGenUI engine logs to Harmony hilog with proper formatting.
 * It also provides a callback interface for displaying logs in the UI.
 */
export class PlaygroundRuntimeLogger implements IAGenUILogger {
  private static readonly TAG: string = 'AGenUI-Engine';
  private static readonly DOMAIN: number = 0x0000;
  private static readonly LOG_DIR_NAME: string = 'agenui_log';
  
  /**
   * Callback interface for receiving log messages in the UI
   */
  public logCallback?: (level: number, tag: string, func: string, line: number, message: string) => void;
  
  private logDirectory: string = '';
  private currentLogFile: string = '';
  
  constructor(context: common.UIAbilityContext, callback?: (level: number, tag: string, func: string, line: number, message: string) => void) {
    this.logCallback = callback;
    this.setupLogDirectory(context);
    this.createNewLogFile();
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
    
    // Asynchronously save log to file
    this.saveLogToFile(level, timestamp, tag, func, line, message);
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
  
  /**
   * Setup log directory in app's files directory
   */
  private setupLogDirectory(context: common.UIAbilityContext): void {
    try {
      const filesDir = context.filesDir;
      this.logDirectory = `${filesDir}/${PlaygroundRuntimeLogger.LOG_DIR_NAME}`;
      
      if (!fs.accessSync(this.logDirectory)) {
        fs.mkdirSync(this.logDirectory);
      }
    } catch (error) {
      hilog.error(PlaygroundRuntimeLogger.DOMAIN, PlaygroundRuntimeLogger.TAG, 
        'Failed to create log directory: %{public}s', JSON.stringify(error));
    }
  }
  
  /**
   * Create a new log file for this session
   */
  private createNewLogFile(): void {
    if (!this.logDirectory) {
      return;
    }
    
    const date = new Date();
    const fileName = `${this.getFullTimestamp(date)}.log`;
    this.currentLogFile = `${this.logDirectory}/${fileName}`;
    
    hilog.info(PlaygroundRuntimeLogger.DOMAIN, PlaygroundRuntimeLogger.TAG, 
      'Created new log file: %{public}s', this.currentLogFile);
  }
  
  /**
   * Asynchronously save log to file
   */
  private saveLogToFile(level: number, timestamp: string, tag: string, func: string, line: number, message: string): void {
    if (!this.currentLogFile) {
      return;
    }
    
    // Use setTimeout to make it asynchronous
    setTimeout(() => {
      try {
        const levelName = PlaygroundRuntimeLogger.getLevelName(level);
        const logContent = `[AGenUI/${levelName}] ${timestamp} [${tag}:${line}] ${func} - ${message}\n`;
        
        // Check if file exists, then append or create
        if (fs.accessSync(this.currentLogFile)) {
          // File exists, append to it
          const fileDescriptor: number = fs.openSync(this.currentLogFile, fs.OpenMode.READ_WRITE | fs.OpenMode.APPEND).fd;
          fs.writeSync(fileDescriptor, logContent);
          fs.closeSync(fileDescriptor);
        } else {
          // File doesn't exist, create new
          const fileDescriptor: number = fs.openSync(
            this.currentLogFile,
            fs.OpenMode.CREATE | fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC
          ).fd;
          fs.writeSync(fileDescriptor, logContent);
          fs.closeSync(fileDescriptor);
        }
      } catch (error) {
        hilog.error(PlaygroundRuntimeLogger.DOMAIN, PlaygroundRuntimeLogger.TAG, 
          'Failed to save log file: %{public}s', JSON.stringify(error));
      }
    }, 0);
  }
  
  /**
   * Get full timestamp for file name
   * Format: yyyyMMdd_HHmmss_SSS (e.g., 20260510_143025_123)
   */
  private getFullTimestamp(date: Date): string {
    const year = date.getFullYear();
    const month = this.padZero(date.getMonth() + 1, 2);
    const day = this.padZero(date.getDate(), 2);
    const hours = this.padZero(date.getHours(), 2);
    const minutes = this.padZero(date.getMinutes(), 2);
    const seconds = this.padZero(date.getSeconds(), 2);
    const milliseconds = this.padZero(date.getMilliseconds(), 3);
    return `${year}${month}${day}_${hours}${minutes}${seconds}_${milliseconds}`;
  }
}
