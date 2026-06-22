package com.amap.agenuiplayground.stability;

import android.os.Debug;

/**
 * Memory monitor for stability testing.
 * Tracks native heap and Java heap, detects potential leaks.
 */
public class MemoryMonitor {
    private long baselineNative;
    private long baselineJava;
    private long peakNative;
    private long peakJava;

    public void recordBaseline() {
        System.gc();
        try { Thread.sleep(100); } catch (InterruptedException ignored) {}
        baselineNative = Debug.getNativeHeapAllocatedSize();
        baselineJava = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        peakNative = baselineNative;
        peakJava = baselineJava;
    }

    public long getNativeHeapMb() {
        long current = Debug.getNativeHeapAllocatedSize();
        if (current > peakNative) peakNative = current;
        return current / (1024 * 1024);
    }

    public long getJavaHeapMb() {
        long current = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        if (current > peakJava) peakJava = current;
        return current / (1024 * 1024);
    }

    public long getTotalMb() {
        return getNativeHeapMb() + getJavaHeapMb();
    }

    public long getBaselineTotalMb() {
        return (baselineNative + baselineJava) / (1024 * 1024);
    }

    public long getPeakTotalMb() {
        return (peakNative + peakJava) / (1024 * 1024);
    }

    public long getGrowthMb() {
        long currentTotal = Debug.getNativeHeapAllocatedSize() +
                (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory());
        return (currentTotal - baselineNative - baselineJava) / (1024 * 1024);
    }

    public String getSummary() {
        return String.format("Baseline: %dMB, Current: %dMB, Peak: %dMB, Growth: %dMB",
                getBaselineTotalMb(), getTotalMb(), getPeakTotalMb(), getGrowthMb());
    }
}
