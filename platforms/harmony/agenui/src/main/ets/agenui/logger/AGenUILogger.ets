import { IAGenUILogger } from './IAGenUILogger';
import { hilog } from '@kit.PerformanceAnalysisKit';

/**
 * @brief AGenUI Logger Manager for HarmonyOS
 * 
 * Manages the lifecycle of C++ IRuntimeLogger implementation, forwarding
 * log calls from C++ side to ArkTS side logger.
 * 
 * Dependency Inversion Principle:
 * - C++ modules only depend on IRuntimeLogger abstract interface
 * - Internal implementation uses C++ wrapper class to implement this interface, injected into C++ engine
 * - ArkTS logger acts as pass-through channel, with concrete logic implemented by consumer
 */
export class AGenUILogger {
  private static instance: AGenUILogger | undefined = undefined;
  private customLogger: IAGenUILogger | undefined = undefined;
  private domain: number = 0x0000;
  private tag: string = 'AGenUI';
  
  private constructor() {
    // Private constructor for singleton
  }
  
  /**
   * Returns the AGenUILogger singleton instance
   */
  public static getInstance(): AGenUILogger {
    if (!AGenUILogger.instance) {
      AGenUILogger.instance = new AGenUILogger();
    }
    return AGenUILogger.instance;
  }
  
  /**
   * Set the custom logger that will receive log callbacks
   * 
   * @param customLogger Custom logger implementation
   */
  public setCustomLogger(customLogger: IAGenUILogger): void {
    this.customLogger = customLogger;
  }
  
  /**
   * Handle log message from native C++ layer
   * This method is called from NAPI layer
   * 
   * @param level Log level
   * @param tag Log tag
   * @param func Function name
   * @param line Line number
   * @param message Log message
   */
  public onLogFromNative(level: number, tag: string, func: string, line: number, message: string): void {
    if (this.customLogger) {
      try {
        this.customLogger.onLog(level, tag, func, line, message);
      } catch (error) {
        hilog.error(this.domain, this.tag, 'Error in logger delegate: %{public}s', JSON.stringify(error));
      }
    } else {
      // Fallback to default HarmonyOS logging
      this.logToHiLog(level, tag, func, line, message);
    }
  }
  
  /**
   * Fallback logging to HarmonyOS hilog system
   */
  private logToHiLog(level: number, tag: string, func: string, line: number, message: string): void {
    const fullMessage = `[${func}@${line}] ${message}`;
    const logTag = tag && tag.length > 0 ? tag : this.tag;
    
    switch (level) {
      case 0: // DEBUG
        hilog.debug(this.domain, logTag, '%{public}s', fullMessage);
        break;
      case 1: // INFO
        hilog.info(this.domain, logTag, '%{public}s', fullMessage);
        break;
      case 2: // WARN
        hilog.warn(this.domain, logTag, '%{public}s', fullMessage);
        break;
      case 3: // ERROR
        hilog.error(this.domain, logTag, '%{public}s', fullMessage);
        break;
      case 4: // FATAL
        hilog.fatal(this.domain, logTag, '%{public}s', fullMessage);
        break;
      case 5: // PERFORMANCE
        hilog.debug(this.domain, logTag, '[PERF] %{public}s', fullMessage);
        break;
      default:
        hilog.debug(this.domain, logTag, '%{public}s', fullMessage);
        break;
    }
  }
}
