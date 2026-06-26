import Foundation
import AGenUI

/// Defines stress + realistic scenarios for stability testing.
enum StabilityScenario: String, CaseIterable {
    // Stress scenarios (original 7 + 1 robustness)
    case sessionStorm = "SESSION_STORM"
    case streamMarathon = "STREAM_MARATHON"
    case multiSurface = "MULTI_SURFACE"
    case actionFlood = "ACTION_FLOOD"
    case themeSwitch = "THEME_SWITCH"
    case interruptRecover = "INTERRUPT_RECOVER"
    case extremeRender = "EXTREME_RENDER"
    case sdkRobustness = "SDK_ROBUSTNESS"
    case sdkInterfaceStability = "SDK_INTERFACE_STABILITY"

    // Realistic scenarios (10 new)
    case realisticArticleStream = "REALISTIC_ARTICLE_STREAM"
    case realisticMultiCard = "REALISTIC_MULTI_CARD"
    case realisticFormFill = "REALISTIC_FORM_FILL"
    case realisticChartRefresh = "REALISTIC_CHART_REFRESH"
    case realisticLongList = "REALISTIC_LONG_LIST"
    case realisticPageSwitch = "REALISTIC_PAGE_SWITCH"
    case realisticTabNavigation = "REALISTIC_TAB_NAVIGATION"
    case realisticLottieCarousel = "REALISTIC_LOTTIE_CAROUSEL"
    case realisticMixedDashboard = "REALISTIC_MIXED_DASHBOARD"
    case realisticErrorRecovery = "REALISTIC_ERROR_RECOVERY"

    // Meta-scenarios
    case allCombined = "ALL_COMBINED"
    case allStress = "ALL_STRESS"
    case allRealistic = "ALL_REALISTIC"

    var isRealistic: Bool {
        switch self {
        case .realisticArticleStream, .realisticMultiCard, .realisticFormFill,
             .realisticChartRefresh, .realisticLongList, .realisticPageSwitch,
             .realisticTabNavigation, .realisticLottieCarousel,
             .realisticMixedDashboard, .realisticErrorRecovery:
            return true
        default:
            return false
        }
    }

    var isStress: Bool {
        switch self {
        case .sessionStorm, .streamMarathon, .multiSurface, .actionFlood,
             .themeSwitch, .interruptRecover, .extremeRender, .sdkRobustness,
             .sdkInterfaceStability:
            return true
        default:
            return false
        }
    }

    var isMeta: Bool {
        switch self {
        case .allCombined, .allStress, .allRealistic:
            return true
        default:
            return false
        }
    }

    /// All individual stress scenarios
    static var stressScenarios: [StabilityScenario] {
        return allCases.filter { $0.isStress }
    }

    /// All individual realistic scenarios
    static var realisticScenarios: [StabilityScenario] {
        return allCases.filter { $0.isRealistic }
    }

    /// All non-meta scenarios
    static var individualScenarios: [StabilityScenario] {
        return allCases.filter { !$0.isMeta }
    }

    static func parse(_ name: String?) -> StabilityScenario {
        guard let name = name?.uppercased().replacingOccurrences(of: "-", with: "_") else {
            return .allCombined
        }
        return StabilityScenario(rawValue: name) ?? .allCombined
    }
}

/// Result of executing one round.
struct ScenarioRoundResult {
    let fixture: String?
    let error: String?
    var isSuccess: Bool { error == nil }
}

/// Executes stress scenarios against the AGenUI SDK.
class StabilityScenarioEngine {
    private var fixtureFiles: [String: [String]] = [:]
    private var interfaceLogger: InterfaceLogger?
    private let fixtureCategories = [
        "extreme_components", "extreme_data", "extreme_stream",
        "extreme_lifecycle", "extreme_interaction"
    ]

    init() {
        loadFixtureManifest()
    }

    func setFixtureFilter(_ filter: [String]) {
        guard !filter.isEmpty else { return }
        for (category, files) in fixtureFiles {
            fixtureFiles[category] = files.filter { path in
                filter.contains(where: { path == $0 || path.hasSuffix("/\($0)") })
            }
        }
        fixtureFiles = fixtureFiles.filter { !$0.value.isEmpty }
        let total = fixtureFiles.values.reduce(0) { $0 + $1.count }
        NSLog("[StabilityEngine] Fixture filter applied: %d files retained", total)
    }

    private func loadFixtureManifest() {
        for category in fixtureCategories {
            guard let dirPath = Bundle.main.path(forResource: category, ofType: nil, inDirectory: "stability_fixtures") else { continue }
            let dirURL = URL(fileURLWithPath: dirPath)
            guard let files = try? FileManager.default.contentsOfDirectory(at: dirURL, includingPropertiesForKeys: nil)
                .filter({ $0.pathExtension == "json" })
                .map({ "\(category)/\($0.lastPathComponent)" }) else { continue }
            fixtureFiles[category] = files
        }
    }

    /// Select a random non-blacklisted scenario based on the meta-scenario mode.
    func selectCombinedScenario(mode: StabilityScenario, crashTracker: StabilityCrashTracker) -> StabilityScenario? {
        let pool: [StabilityScenario]
        switch mode {
        case .allStress:
            pool = StabilityScenario.stressScenarios
        case .allRealistic:
            pool = StabilityScenario.realisticScenarios
        default: // allCombined
            pool = StabilityScenario.individualScenarios
        }
        let available = pool.filter { !crashTracker.isBlacklisted($0.rawValue) }
        return available.randomElement()
    }

    /// Execute one round of the given scenario. Must be called on main thread.
    /// Only handles stress scenarios. Realistic scenarios use RealisticScenarioEngine.
    func executeRound(_ scenario: StabilityScenario) -> ScenarioRoundResult {
        guard scenario.isStress else {
            return ScenarioRoundResult(fixture: nil, error: "Use RealisticScenarioEngine for realistic scenarios")
        }
        do {
            switch scenario {
            case .sessionStorm:
                try executeSessionStorm()
                return ScenarioRoundResult(fixture: nil, error: nil)
            case .streamMarathon:
                try executeStreamMarathon()
                return ScenarioRoundResult(fixture: nil, error: nil)
            case .multiSurface:
                try executeMultiSurface()
                return ScenarioRoundResult(fixture: nil, error: nil)
            case .actionFlood:
                try executeActionFlood()
                return ScenarioRoundResult(fixture: nil, error: nil)
            case .themeSwitch:
                try executeThemeSwitch()
                return ScenarioRoundResult(fixture: nil, error: nil)
            case .interruptRecover:
                try executeInterruptRecover()
                return ScenarioRoundResult(fixture: nil, error: nil)
            case .extremeRender:
                let fixture = try executeExtremeRender()
                return ScenarioRoundResult(fixture: fixture, error: nil)
            case .sdkRobustness:
                let result = executeSdkRobustness()
                return ScenarioRoundResult(fixture: result, error: nil)
            case .sdkInterfaceStability:
                let result = executeSdkInterfaceStability()
                return ScenarioRoundResult(fixture: result, error: nil)
            default:
                return ScenarioRoundResult(fixture: nil, error: "\(scenario.rawValue) not handled by StabilityScenarioEngine")
            }
        } catch {
            return ScenarioRoundResult(fixture: nil, error: error.localizedDescription)
        }
    }

    // MARK: - S1: Session Storm
    /// Rapidly create and destroy SurfaceManagers (10 iterations)
    private func executeSessionStorm() throws {
        for i in 0..<10 {
            autoreleasepool {
                let sm = SurfaceManager()
                sm.beginTextStream()
                let json = buildCreateSurfaceJSON(surfaceId: "storm-\(i)")
                sm.receiveTextChunk(json)
                sm.endTextStream()
            }
        }
    }

    // MARK: - S2: Stream Marathon
    /// Long streaming session with 100 data model updates on a single surface
    private func executeStreamMarathon() throws {
        let sm = SurfaceManager()
        sm.beginTextStream()
        sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "marathon"))

        for i in 0..<100 {
            let updateJSON = buildUpdateDataModelJSON(surfaceId: "marathon", key: "counter", value: "\(i)")
            sm.receiveTextChunk(updateJSON)
        }

        sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "marathon"))
        sm.endTextStream()
    }

    // MARK: - S3: Multi Surface
    /// Create 5 surfaces, random updates, then delete all
    private func executeMultiSurface() throws {
        let sm = SurfaceManager()
        sm.beginTextStream()

        // Create 5 surfaces
        for i in 0..<5 {
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "multi-\(i)"))
        }

        // 20 random updates across surfaces
        for _ in 0..<20 {
            let idx = Int.random(in: 0..<5)
            let update = buildUpdateComponentsJSON(surfaceId: "multi-\(idx)", text: "Update \(Int.random(in: 0..<1000))")
            sm.receiveTextChunk(update)
        }

        // Delete all
        for i in 0..<5 {
            sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "multi-\(i)"))
        }

        sm.endTextStream()
    }

    // MARK: - S4: Action Flood
    /// Rapid data model updates simulating user input (50 iterations)
    private func executeActionFlood() throws {
        let sm = SurfaceManager()
        sm.beginTextStream()
        sm.receiveTextChunk(buildCreateSurfaceWithInputJSON(surfaceId: "flood"))

        for i in 0..<50 {
            let update = buildUpdateDataModelJSON(surfaceId: "flood", key: "input_\(i % 5)", value: "value_\(i)")
            sm.receiveTextChunk(update)
        }

        sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "flood"))
        sm.endTextStream()
    }

    // MARK: - S5: Theme Switch
    /// Rapid day/night mode toggling (20 iterations)
    private func executeThemeSwitch() throws {
        for i in 0..<20 {
            AGenUISDK.setDayNightMode(i % 2 == 0 ? "light" : "dark")
        }
    }

    // MARK: - S6: Interrupt Recover
    /// Start a stream, interrupt mid-way (nil the SM), then create a fresh one and complete
    private func executeInterruptRecover() throws {
        // Phase 1: Start and interrupt
        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "interrupt"))
            // Intentionally NOT calling endTextStream — simulates crash/interrupt
        }

        // Phase 2: Recover with a new surface manager
        autoreleasepool {
            let sm2 = SurfaceManager()
            sm2.beginTextStream()
            sm2.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "recover"))
            sm2.receiveTextChunk(buildUpdateComponentsJSON(surfaceId: "recover", text: "Recovered successfully"))
            sm2.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "recover"))
            sm2.endTextStream()
        }
    }

    // MARK: - S7: Extreme Render
    /// Load a random stability fixture and render it
    private func executeExtremeRender() throws -> String {
        let allFiles = fixtureFiles.values.flatMap { $0 }
        guard !allFiles.isEmpty else {
            throw NSError(domain: "StabilityEngine", code: 1, userInfo: [NSLocalizedDescriptionKey: "No fixture files found"])
        }

        let fixture = allFiles.randomElement()!
        let components = fixture.split(separator: "/")
        guard components.count == 2 else {
            throw NSError(domain: "StabilityEngine", code: 2, userInfo: [NSLocalizedDescriptionKey: "Invalid fixture path: \(fixture)"])
        }

        let category = String(components[0])
        let filename = String(components[1])
        let name = filename.replacingOccurrences(of: ".json", with: "")

        guard let path = Bundle.main.path(forResource: name, ofType: "json", inDirectory: "stability_fixtures/\(category)"),
              let data = FileManager.default.contents(atPath: path),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw NSError(domain: "StabilityEngine", code: 3, userInfo: [NSLocalizedDescriptionKey: "Failed to load fixture: \(fixture)"])
        }

        let sm = SurfaceManager()
        sm.beginTextStream()

        if let messages = json["messages"] as? [[String: Any]] {
            for msg in messages {
                if let msgData = try? JSONSerialization.data(withJSONObject: msg),
                   let msgStr = String(data: msgData, encoding: .utf8) {
                    sm.receiveTextChunk(msgStr)
                }
            }
        } else if let payload = json["payload"] as? String {
            // Stream in 100-byte chunks
            let chunkSize = 100
            var offset = payload.startIndex
            while offset < payload.endIndex {
                let end = payload.index(offset, offsetBy: chunkSize, limitedBy: payload.endIndex) ?? payload.endIndex
                sm.receiveTextChunk(String(payload[offset..<end]))
                offset = end
            }
        }

        sm.endTextStream()
        return fixture
    }

    // MARK: - S8: SDK Robustness
    /// 12 defensive sub-cases testing API misuse tolerance.
    /// Each sub-case exercises a different API misuse pattern.
    /// If any causes a crash, the crash log will identify which sub-case failed.
    private func executeSdkRobustness() -> String {
        var results: [String] = []
        var passed = 0
        let total = 12

        // R1: operations_after_end — call APIs after endTextStream
        NSLog("[StabilityEngine] R1: operations_after_end")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r1-surf"))
            sm.endTextStream()
            // Call APIs after stream ended
            sm.beginTextStream()
            sm.receiveTextChunk("{\"version\":\"v0.9\"}")
            sm.endTextStream()
            sm.receiveTextChunk("{\"version\":\"v0.9\"}")
            sm.endTextStream()
        }
        passed += 1
        results.append("R1:OK")

        // R2: double_begin_stream
        NSLog("[StabilityEngine] R2: double_begin_stream")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.beginTextStream() // second begin
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r2-surf"))
            sm.endTextStream()
        }
        passed += 1
        results.append("R2:OK")

        // R3: receive_without_begin
        NSLog("[StabilityEngine] R3: receive_without_begin")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r3-surf"))
            sm.endTextStream()
        }
        passed += 1
        results.append("R3:OK")

        // R4: end_without_begin
        NSLog("[StabilityEngine] R4: end_without_begin")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.endTextStream()
        }
        passed += 1
        results.append("R4:OK")

        // R5: double_end_stream
        NSLog("[StabilityEngine] R5: double_end_stream")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r5-surf"))
            sm.endTextStream()
            sm.endTextStream() // second end
        }
        passed += 1
        results.append("R5:OK")

        // R6: removeAllListeners_then_operate
        NSLog("[StabilityEngine] R6: removeAllListeners_then_operate")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.removeAllListeners()
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r6-surf"))
            sm.endTextStream()
            sm.removeAllListeners()
        }
        passed += 1
        results.append("R6:OK")

        // R7: malformed_json — various broken payloads
        NSLog("[StabilityEngine] R7: malformed_json")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            let malformed = [
                "{broken json",
                "{{{{",
                "{\"version\":\"v0.9\",\"createSurface\":{}}",
                "{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":null}}",
                "<html>not json</html>",
                String(repeating: "x", count: 10000),
                "{\"version\":\"v99.99\",\"unknownOp\":{\"surfaceId\":\"r7\"}}",
                "null",
                "[]",
                "0"
            ]
            for payload in malformed {
                sm.receiveTextChunk(payload)
            }
            sm.endTextStream()
        }
        passed += 1
        results.append("R7:OK")

        // R8: empty_and_whitespace
        NSLog("[StabilityEngine] R8: empty_and_whitespace")
        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.receiveTextChunk("")
            sm.receiveTextChunk("   ")
            sm.receiveTextChunk("\n\n")
            sm.receiveTextChunk("\t")
            sm.endTextStream()
        }
        passed += 1
        results.append("R8:OK")

        // R9: rapid_lifecycle — many begin/end cycles without data
        NSLog("[StabilityEngine] R9: rapid_lifecycle")
        autoreleasepool {
            let sm = SurfaceManager()
            for _ in 0..<50 {
                sm.beginTextStream()
                sm.endTextStream()
            }
        }
        passed += 1
        results.append("R9:OK")

        // R10: remove_unregistered_listener
        NSLog("[StabilityEngine] R10: remove_unregistered_listener")
        autoreleasepool {
            let sm = SurfaceManager()
            let unregistered = RobustnessListener()
            sm.removeListener(unregistered) // never added
            sm.removeListener(unregistered) // twice
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r10-surf"))
            sm.endTextStream()
        }
        passed += 1
        results.append("R10:OK")

        // R11: rapid_release — autoreleasepool + immediate reuse, some without endTextStream
        NSLog("[StabilityEngine] R11: rapid_release")
        for i in 0..<10 {
            autoreleasepool {
                let sm = SurfaceManager()
                sm.beginTextStream()
                sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r11-\(i)"))
                // intentionally skip endTextStream on odd iterations
                if i % 2 == 0 {
                    sm.endTextStream()
                }
            }
        }
        passed += 1
        results.append("R11:OK")

        // R12: listener_operations_interleaved — add/remove listener mid-stream
        NSLog("[StabilityEngine] R12: listener_operations_interleaved")
        autoreleasepool {
            let sm = SurfaceManager()
            let listener = RobustnessListener()
            sm.addListener(listener)
            sm.removeListener(listener)
            sm.addListener(listener)
            sm.beginTextStream()
            sm.removeListener(listener) // remove during stream
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "r12-surf"))
            sm.addListener(listener) // re-add during stream
            sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "r12-surf"))
            sm.endTextStream()
            sm.removeAllListeners()
        }
        passed += 1
        results.append("R12:OK")

        let summary = "sdk_robustness[\(results.joined(separator: ","))] \(passed)/\(total) passed"
        NSLog("[StabilityEngine] %@", summary)
        return summary
    }

    /// SDK interface stability — repeated and interleaved public API usage.
    /// Focus is on crash/freeze resistance rather than semantic correctness.
    private func executeSdkInterfaceStability() -> String {
        var results: [String] = []
        var passed = 0
        let total = 10

        autoreleasepool {
            let ok = AGenUISDK.registerDefaultTheme(buildThemeJSON("ffffff"), designToken: buildDesignTokenJSON("#ffffff"))
            let invalidTheme = AGenUISDK.registerDefaultTheme("{invalid", designToken: buildDesignTokenJSON("#000000"))
            let invalidToken = AGenUISDK.registerDefaultTheme(buildThemeJSON("000000"), designToken: "{invalid")
            passed += 1
            results.append("I1:OK(ok=\(ok.result),invalidTheme=\(invalidTheme.result),invalidToken=\(invalidToken.result))")
        }

        autoreleasepool {
            let valid = AGenUISDK.setPathConfig("{\"templateDir\":\"/tmp/agenui-stability\"}")
            let invalid = AGenUISDK.setPathConfig("{invalid")
            passed += 1
            results.append("I2:OK(valid=\(valid.result),invalid=\(invalid.result))")
        }

        autoreleasepool {
            interfaceLogger = InterfaceLogger()
            AGenUISDK.setCustomLogger(interfaceLogger)
            for level in 0...5 {
                if let swiftLevel = Logger.Level(rawValue: level) {
                    AGenUISDK.setMinLogLevel(swiftLevel)
                    let observed = AGenUISDK.getMinLogLevel()
                    if observed.rawValue != level {
                        interfaceLogger?.appendSignal("mismatch:\(level)->\(observed.rawValue)")
                    }
                }
            }
            let signals = interfaceLogger?.snapshotSignals() ?? []
            AGenUISDK.setCustomLogger(nil)
            interfaceLogger = nil
            passed += 1
            results.append("I3:OK(\(signals.isEmpty ? "stable" : signals.joined(separator: ";")))") 
        }

        autoreleasepool {
            var version = ""
            for _ in 0..<20 {
                version = AGenUISDK.getVersion()
            }
            passed += 1
            results.append("I4:OK(\(version.isEmpty ? "empty" : version))")
        }

        autoreleasepool {
            for i in 0..<20 {
                AGenUISDK.setDayNightMode(i % 2 == 0 ? "light" : "dark")
            }
            AGenUISDK.setDayNightMode("invalid-mode")
            passed += 1
            results.append("I5:OK")
        }

        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "i6-surf"))
            sm.endTextStream()
            for _ in 0..<10 {
                sm.invalidateFunctionCallValues()
            }
            passed += 1
            results.append("I6:OK")
        }

        autoreleasepool {
            let sm = SurfaceManager()
            let listener = InterfaceListener()
            sm.addListener(listener)
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "i7-surf"))
            sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "i7-surf"))
            sm.endTextStream()
            sm.removeListener(listener)
            passed += 1
            results.append("I7:OK(\(listener.lastEvent))")
        }

        autoreleasepool {
            let sm = SurfaceManager()
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "i8-a"))
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "i8-b"))
            sm.endTextStream()
            sm.beginTextStream()
            sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "i8-a"))
            sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "i8-b"))
            sm.endTextStream()
            passed += 1
            results.append("I8:OK")
        }

        autoreleasepool {
            for i in 0..<8 {
                let sm = SurfaceManager()
                sm.beginTextStream()
                sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "i9-\(i)"))
                sm.endTextStream()
                AGenUISDK.setDayNightMode(i % 2 == 0 ? "light" : "dark")
                sm.invalidateFunctionCallValues()
                sm.removeAllListeners()
            }
            passed += 1
            results.append("I9:OK")
        }

        autoreleasepool {
            interfaceLogger = InterfaceLogger()
            AGenUISDK.setCustomLogger(interfaceLogger)
            let sm = SurfaceManager()
            let listener = InterfaceListener()
            sm.addListener(listener)
            sm.beginTextStream()
            sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: "i10-surf"))
            sm.receiveTextChunk(buildDeleteSurfaceJSON(surfaceId: "i10-surf"))
            sm.endTextStream()
            sm.removeListener(listener)
            let logCount = interfaceLogger?.snapshotSignals().count ?? 0
            AGenUISDK.setCustomLogger(nil)
            interfaceLogger = nil
            passed += 1
            results.append("I10:OK(events=\(listener.lastEvent),logs=\(logCount))")
        }

        let summary = "sdk_interface_stability[\(results.joined(separator: ","))] \(passed)/\(total) passed"
        NSLog("[StabilityEngine] %@", summary)
        return summary
    }

    // MARK: - JSON Builders

    private func buildCreateSurfaceJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}
        """
    }

    private func buildDeleteSurfaceJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","deleteSurface":{"surfaceId":"\(surfaceId)"}}
        """
    }

    private func buildUpdateDataModelJSON(surfaceId: String, key: String, value: String) -> String {
        return """
        {"version":"v0.9","updateDataModel":{"surfaceId":"\(surfaceId)","dataModel":{"\(key)":"\(value)"}}}
        """
    }

    private func buildUpdateComponentsJSON(surfaceId: String, text: String) -> String {
        return """
        {"version":"v0.9","updateComponents":{"surfaceId":"\(surfaceId)","components":[{"type":"Text","id":"txt-\(surfaceId)","properties":{"text":"\(text)"}}]}}
        """
    }

    private func buildCreateSurfaceWithInputJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json","sendDataModel":true}}
        """
    }

    private func buildThemeJSON(_ colorHexWithoutHash: String) -> String {
        return """
        {"colors":{"textPrimary":"#\(colorHexWithoutHash)","backgroundPrimary":"#\(colorHexWithoutHash)"}}
        """
    }

    private func buildDesignTokenJSON(_ colorHex: String) -> String {
        return """
        {"color":{"text":{"primary":"\(colorHex)"},"bg":{"primary":"\(colorHex)"}}}
        """
    }
}

/// Helper listener for SDK robustness testing
private class RobustnessListener: NSObject, SurfaceManagerListener {
    func onCreateSurface(_ surface: Surface) {}
    func onDeleteSurface(_ surface: Surface) {}
}

private final class InterfaceListener: NSObject, SurfaceManagerListener {
    var lastEvent = "none"

    func onCreateSurface(_ surface: Surface) {
        lastEvent = "created"
    }

    func onDeleteSurface(_ surface: Surface) {
        lastEvent = "deleted"
    }
}

private final class InterfaceLogger: NSObject, LoggerDelegate {
    private let lock = NSLock()
    private var storedSignals: [String] = []

    func appendSignal(_ signal: String) {
        lock.lock()
        storedSignals.append(signal)
        lock.unlock()
    }

    func snapshotSignals() -> [String] {
        lock.lock()
        let snapshot = storedSignals
        lock.unlock()
        return snapshot
    }

    func onLog(level: Logger.Level, tag: String, func: String, line: Int, message: String) {
        appendSignal("\(level.rawValue)")
    }
}
