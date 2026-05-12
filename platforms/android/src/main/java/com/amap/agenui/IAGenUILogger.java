package com.amap.agenui;

/**
 * @brief AGenUI Logger Interface
 * 
 * Defines the logging callback interface that platform implementations must implement.
 * This follows the Dependency Inversion Principle (DIP):
 * - C++ modules depend on abstract IRuntimeLogger interface
 * - Concrete implementation is injected from Java/Kotlin layer
 */
public interface IAGenUILogger {
    /**
     * Log callback method
     * 
     * @param level Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL, 5=PERFORMANCE)
     * @param tag Log tag
     * @param func Function name
     * @param line Line number
     * @param message Log message
     */
    void onLog(int level, String tag, String func, int line, String message);
}
