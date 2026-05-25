package com.amap.agenui.function;

/**
 * Context information for a function call.
 *
 * Provides the engine and surface context in which the function call is invoked.
 * Instances are created by the native (C++) layer and passed to Java via JNI.
 */
public class FunctionCallContext {

    /** Instance unique identifier */
    private final int instanceId;

    /** Surface unique identifier */
    private final String surfaceId;

    /**
     * Constructor (called from JNI)
     *
     * @param instanceId  Instance unique identifier
     * @param surfaceId Surface unique identifier
     */
    public FunctionCallContext(int instanceId, String surfaceId) {
        this.instanceId = instanceId;
        this.surfaceId = surfaceId;
    }

    /**
     * @return Instance unique identifier
     */
    public int getInstanceId() {
        return instanceId;
    }

    /**
     * @return Surface unique identifier
     */
    public String getSurfaceId() {
        return surfaceId;
    }
}
