import Foundation
import Darwin

/// Lightweight memory monitor for stability testing.
/// Tracks resident memory (MB) with baseline and peak.
class StabilityMemoryMonitor {
    private(set) var baselineMb: Double = 0
    private(set) var peakMb: Double = 0

    func recordBaseline() {
        baselineMb = getCurrentMb()
        peakMb = baselineMb
    }

    func update() {
        let current = getCurrentMb()
        if current > peakMb {
            peakMb = current
        }
    }

    func getCurrentMb() -> Double {
        var info = mach_task_basic_info()
        var count = mach_msg_type_number_t(MemoryLayout<mach_task_basic_info>.size) / 4
        let kerr: kern_return_t = withUnsafeMutablePointer(to: &info) {
            $0.withMemoryRebound(to: integer_t.self, capacity: Int(count)) {
                task_info(mach_task_self_, task_flavor_t(MACH_TASK_BASIC_INFO), $0, &count)
            }
        }
        if kerr == KERN_SUCCESS {
            return Double(info.resident_size) / 1024.0 / 1024.0
        }
        return 0
    }

    func getGrowthMb() -> Double {
        return getCurrentMb() - baselineMb
    }

    func getSummary() -> String {
        let current = getCurrentMb()
        return String(format: "%.0fMB (baseline: %.0fMB, peak: %.0fMB)", current, baselineMb, peakMb)
    }
}
