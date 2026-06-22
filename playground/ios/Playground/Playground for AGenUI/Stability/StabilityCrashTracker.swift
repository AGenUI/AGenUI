import Foundation

/// Manages crash attribution using write-ahead state files.
/// Before each round, writes crash_state.json. If the process crashes,
/// monitor.sh reads this file to determine which scenario caused the crash.
class StabilityCrashTracker {
    private let outputDir: URL
    private let threshold: Int
    private var registry: [String: CrashEntry] = [:]

    private var stateFileURL: URL { outputDir.appendingPathComponent("crash_state.json") }
    private var registryFileURL: URL { outputDir.appendingPathComponent("crash_registry.json") }
    private var lastCrashFileURL: URL { outputDir.appendingPathComponent("last_crash_scenario.txt") }

    private(set) var lastCrashedScenario: String?

    struct CrashEntry: Codable {
        var crashCount: Int
        var blacklisted: Bool
        var lastCrashAt: String?
        var lastRound: Int?
        var lastFixture: String?
    }

    init(outputDir: URL, threshold: Int) {
        self.outputDir = outputDir
        self.threshold = threshold
        loadRegistry()
    }

    /// Called on app startup. If crash_state.json exists, previous run crashed.
    func onProcessStart(logger: StabilityLogger?) {
        let fm = FileManager.default
        guard fm.fileExists(atPath: stateFileURL.path),
              let data = fm.contents(atPath: stateFileURL.path),
              let state = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return
        }

        let scenario = (state["sub_scenario"] as? String) ?? (state["scenario"] as? String) ?? "unknown"
        let round = state["round"] as? Int ?? 0
        lastCrashedScenario = scenario

        // Increment crash count
        var entry = registry[scenario] ?? CrashEntry(crashCount: 0, blacklisted: false)
        entry.crashCount += 1
        entry.lastCrashAt = ISO8601DateFormatter().string(from: Date())
        entry.lastRound = round
        entry.lastFixture = state["fixture"] as? String
        if entry.crashCount >= threshold {
            entry.blacklisted = true
        }
        registry[scenario] = entry
        saveRegistry()

        // Write durable attribution file
        let crashInfo: [String: Any] = [
            "scenario": scenario,
            "round": round,
            "status": "during execution",
            "detected_at": ISO8601DateFormatter().string(from: Date())
        ]
        if let jsonData = try? JSONSerialization.data(withJSONObject: crashInfo, options: [.sortedKeys]) {
            try? jsonData.write(to: lastCrashFileURL, options: .atomic)
        }

        // Log event
        logger?.logEvent(event: "crash_detected", detail: "crashed_scenario=\(scenario),blacklist=\(getBlacklistSummary())")

        // Delete consumed state file
        try? fm.removeItem(at: stateFileURL)
    }

    /// Write-ahead: called BEFORE executing a round.
    func beforeRound(scenario: String, subScenario: String?, round: Int, fixture: String?) {
        var state: [String: Any] = [
            "scenario": scenario,
            "round": round,
            "status": "running",
            "started_at": ISO8601DateFormatter().string(from: Date())
        ]
        if let sub = subScenario { state["sub_scenario"] = sub }
        if let fix = fixture { state["fixture"] = fix }

        if let data = try? JSONSerialization.data(withJSONObject: state, options: [.sortedKeys]) {
            try? data.write(to: stateFileURL, options: .atomic)
        }
    }

    /// Called AFTER a round completes successfully.
    func afterRound() {
        let fm = FileManager.default
        guard fm.fileExists(atPath: stateFileURL.path),
              let data = fm.contents(atPath: stateFileURL.path),
              var state = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }

        state["status"] = "completed"
        state["completed_at"] = ISO8601DateFormatter().string(from: Date())

        if let newData = try? JSONSerialization.data(withJSONObject: state, options: [.sortedKeys]) {
            try? newData.write(to: stateFileURL, options: .atomic)
        }
    }

    /// Called on clean exit (duration reached or max rounds).
    func markCleanExit() {
        try? FileManager.default.removeItem(at: stateFileURL)
    }

    func isBlacklisted(_ scenario: String) -> Bool {
        return registry[scenario]?.blacklisted == true
    }

    func getAvailableScenarios(from scenarios: [String]) -> [String] {
        return scenarios.filter { !isBlacklisted($0) }
    }

    func getBlacklistSummary() -> String {
        let blacklisted = registry.filter { $0.value.blacklisted }.map { "\($0.key)(\($0.value.crashCount))" }
        return blacklisted.isEmpty ? "none" : blacklisted.joined(separator: ",")
    }

    func resetRegistry() {
        registry.removeAll()
        saveRegistry()
    }

    // MARK: - Private

    private func loadRegistry() {
        let fm = FileManager.default
        guard fm.fileExists(atPath: registryFileURL.path),
              let data = fm.contents(atPath: registryFileURL.path),
              let decoded = try? JSONDecoder().decode([String: CrashEntry].self, from: data) else { return }
        registry = decoded
    }

    private func saveRegistry() {
        if let data = try? JSONEncoder().encode(registry) {
            try? data.write(to: registryFileURL, options: .atomic)
        }
    }
}
