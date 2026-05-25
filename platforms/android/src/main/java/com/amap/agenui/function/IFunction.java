package com.amap.agenui.function;

/**
 * Function abstract interface
 *
 * Defines the interface that all Functions must implement.
 * Replaces the original ISkill architecture.
 */
public interface IFunction {

    /**
     * Executes the Function
     *
     * @param context    Call context containing engineId and surfaceId
     * @param jsonString Function parameters (JSON string)
     * @return Execution result
     */
    FunctionResult execute(FunctionCallContext context, String jsonString);

    /**
     * Returns the Function configuration
     *
     * @return Function configuration object
     */
    FunctionConfig getConfig();
}
