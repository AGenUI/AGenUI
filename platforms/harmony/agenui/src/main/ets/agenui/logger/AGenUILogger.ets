import { IAGenUILogger } from './IAGenUILogger';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { setMinLogLevel as nativeSetMinLogLevel } from 'liba2ui-capi.so';

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
  // Log level constants (mirror agenui::LogLevel)
  public static readonly LEVEL_DEBUG: number = 0;
  public static readonly LEVEL_INFO: number = 1;
  public static readonly LEVEL_WARN: number = 2;
  public static readonly LEVEL_ERROR: number = 3;
  public static readonly LEVEL_FATAL: number = 4;
  public static readonly LEVEL_PERFORMANCE: number = 5;

  private static instance: AGenUILogger | undefined = undefined;
  private customLogger: IAGenUILogger | undefined = undefined;
  private domain: number = 0x0000;
  private tag: string = 'AGenUI';
  // Read by the NAPI bridge on every C++ log emission (via getMinLogLevel),
  // and applied in onLogFromNative as a JS-side safety net.
  private minLogLevel: number = AGenUILogger.LEVEL_DEBUG;

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
   * Set the minimum log level. Messages with level below this threshold are filtered
   * out — on the C++ side via IRuntimeLogger::getMinLevel() (called from the LOG_LEVEL
   * macro before any vsnprintf), and on the ArkTS side in onLogFromNative.
   *
   * @param level One of AGenUILogger.LEVEL_DEBUG(0) ... LEVEL_PERFORMANCE(5).
   *              Out-of-range values fall back to LEVEL_DEBUG.
   */
  public setMinLogLevel(level: number): void {
    if (level < AGenUILogger.LEVEL_DEBUG || level > AGenUILogger.LEVEL_PERFORMANCE) {
      level = AGenUILogger.LEVEL_DEBUG;
    }
    this.minLogLevel = level;
    // Push down to the C++ wrapper so the macro-level filter sees the new threshold
    // without paying a NAPI round-trip per log emission.
    try {
      nativeSetMinLogLevel(level);
    } catch (error) {
      hilog.error(this.domain, this.tag, 'nativeSetMinLogLevel failed: %{public}s', JSON.stringify(error));
    }
  }

  /**
   * Called by the NAPI bridge (RuntimeLoggerImpl::getMinLevel) on every C++ log
   * emission, so it must stay a cheap synchronous getter.
   */
  public getMinLogLevel(): number {
    return this.minLogLevel;
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
    if (level < this.minLogLevel) {
      return;
    }
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
