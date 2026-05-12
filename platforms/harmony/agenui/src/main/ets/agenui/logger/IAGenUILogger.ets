/**
 * @brief AGenUI Logger Interface for HarmonyOS
 * 
 * Defines the logging callback interface that platform implementations must implement.
 * This follows the Dependency Inversion Principle (DIP):
 * - C++ modules depend on abstract IRuntimeLogger interface
 * - Concrete implementation is injected from ArkTS layer
 */
export interface IAGenUILogger {
  /**
   * Log callback method
   * 
   * @param level Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL, 5=PERFORMANCE)
   * @param tag Log tag
   * @param func Function name
   * @param line Line number
   * @param message Log message
   */
  onLog(level: number, tag: string, func: string, line: number, message: string): void;
}
