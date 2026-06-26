import UIKit
import AGenUI

/// Configuration for the stability test, parsed from launch arguments.
struct StabilityTestConfig {
    let scenario: StabilityScenario
    let durationMinutes: Int
    let maxRounds: Int
    let intervalMs: Int
    let crashThreshold: Int
    let restartReason: String?
    let fixtures: [String]?

    static func fromLaunchArgs(_ args: [String]) -> StabilityTestConfig {
        func argValue(_ key: String) -> String? {
            guard let idx = args.firstIndex(of: key), idx + 1 < args.count else { return nil }
            return args[idx + 1]
        }
        let fixturesRaw = argValue("--fixtures")
        let fixturesList: [String]? = fixturesRaw?.split(separator: ",").map(String.init)
        return StabilityTestConfig(
            scenario: StabilityScenario.parse(argValue("--scenario")),
            durationMinutes: Int(argValue("--duration") ?? "") ?? 480,
            maxRounds: Int(argValue("--rounds") ?? "") ?? 0,
            intervalMs: Int(argValue("--interval") ?? "") ?? 100,
            crashThreshold: Int(argValue("--crash-threshold") ?? "") ?? 5,
            restartReason: argValue("--restart-reason"),
            fixtures: fixturesList
        )
    }
}

/// Full-screen ViewController that drives the AGenUI stability test.
/// Supports dual mode:
///   - Stress mode: label-based metrics display (original behavior)
///   - Realistic mode: full-screen surface rendering with floating metrics overlay
class StabilityTestViewController: UIViewController {
    private let config: StabilityTestConfig

    // Components
    private var logger: StabilityLogger!
    private var crashTracker: StabilityCrashTracker!
    private var engine: StabilityScenarioEngine!
    private var realisticEngine: RealisticScenarioEngine!
    private var memoryMonitor: StabilityMemoryMonitor!

    // State
    private var currentRound = 0
    private var errorCount = 0
    private var startTime: Date!
    private var timer: DispatchSourceTimer?
    private var isRunning = false
    private var lastRoundInfo = ""
    private var isRealisticRoundInProgress = false

    // UI — Stress mode (labels)
    private let labelsContainer = UIView()
    private let titleLabel = UILabel()
    private let scenarioLabel = UILabel()
    private let roundLabel = UILabel()
    private let durationLabel = UILabel()
    private let memoryLabel = UILabel()
    private let statusLabel = UILabel()
    private let lastRoundLabel = UILabel()
    private let errorLabel = UILabel()
    private let logPathLabel = UILabel()

    // UI — Realistic mode (full-screen rendering)
    private let surfaceScrollView = UIScrollView()
    private let surfaceContentView = UIView()
    private let metricsOverlay = MetricsOverlayView()

    // UI timer for overlay updates
    private var uiUpdateTimer: Timer?

    init(config: StabilityTestConfig) {
        self.config = config
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupOutputDirectory()
        setupComponents()
        buildUI()
        registerSDKComponents()
        startTest()
    }

    override var prefersStatusBarHidden: Bool { true }

    // MARK: - Setup

    private func setupOutputDirectory() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let stabilityDir = docs.appendingPathComponent("stability")
        try? FileManager.default.createDirectory(at: stabilityDir, withIntermediateDirectories: true)

        logger = StabilityLogger(outputDir: stabilityDir)
        crashTracker = StabilityCrashTracker(outputDir: stabilityDir, threshold: config.crashThreshold)
        memoryMonitor = StabilityMemoryMonitor()
        engine = StabilityScenarioEngine()
        if let filter = config.fixtures {
            engine.setFixtureFilter(filter)
        }
        realisticEngine = RealisticScenarioEngine()

        // Hook realistic engine callbacks
        realisticEngine.onSurfaceCreated = { [weak self] surface in
            self?.addSurfaceView(surface)
        }
        realisticEngine.onSurfaceDeleted = { [weak self] surface in
            self?.removeSurfaceView(surface)
        }
    }

    private func setupComponents() {
        crashTracker.onProcessStart(logger: logger)
        if let crashed = crashTracker.lastCrashedScenario {
            lastRoundInfo = "Prior crash: \(crashed)"
        }
        memoryMonitor.recordBaseline()
    }

    private func registerSDKComponents() {
        AGenUISDK.registerComponent("Lottie") { id, properties in
            return LottieComponent(componentId: id, properties: properties)
        }
        AGenUISDK.registerComponent("Chart") { id, properties in
            return ChartComponent(componentId: id, properties: properties)
        }
        AGenUISDK.registerComponent("Markdown") { id, properties in
            return MarkdownComponent(componentId: id, properties: properties)
        }
    }

    // MARK: - UI

    private func buildUI() {
        view.backgroundColor = UIColor(red: 0x1A/255.0, green: 0x1A/255.0, blue: 0x2E/255.0, alpha: 1.0)

        // --- Surface scroll view (full screen, behind labels) ---
        surfaceScrollView.translatesAutoresizingMaskIntoConstraints = false
        surfaceScrollView.backgroundColor = .white
        surfaceScrollView.isHidden = true
        view.addSubview(surfaceScrollView)

        surfaceContentView.translatesAutoresizingMaskIntoConstraints = false
        surfaceScrollView.addSubview(surfaceContentView)

        NSLayoutConstraint.activate([
            surfaceScrollView.topAnchor.constraint(equalTo: view.topAnchor),
            surfaceScrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            surfaceScrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            surfaceScrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            surfaceContentView.topAnchor.constraint(equalTo: surfaceScrollView.topAnchor),
            surfaceContentView.leadingAnchor.constraint(equalTo: surfaceScrollView.leadingAnchor),
            surfaceContentView.trailingAnchor.constraint(equalTo: surfaceScrollView.trailingAnchor),
            surfaceContentView.widthAnchor.constraint(equalTo: surfaceScrollView.widthAnchor),
        ])

        // --- Labels container (stress mode) ---
        labelsContainer.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(labelsContainer)

        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 12
        stack.alignment = .leading
        stack.translatesAutoresizingMaskIntoConstraints = false

        titleLabel.text = "AGenUI Stability Test"
        titleLabel.font = .boldSystemFont(ofSize: 22)
        titleLabel.textColor = .white

        let labels = [scenarioLabel, roundLabel, durationLabel, memoryLabel, statusLabel, lastRoundLabel, errorLabel, logPathLabel]
        for label in labels {
            label.font = .monospacedSystemFont(ofSize: 14, weight: .regular)
            label.textColor = UIColor(red: 0xE0/255.0, green: 0xE0/255.0, blue: 0xE0/255.0, alpha: 1.0)
            label.numberOfLines = 0
        }
        memoryLabel.textColor = UIColor(red: 0x87/255.0, green: 0xCE/255.0, blue: 0xEB/255.0, alpha: 1.0)
        statusLabel.textColor = .systemGreen
        logPathLabel.font = .monospacedSystemFont(ofSize: 11, weight: .regular)
        logPathLabel.textColor = UIColor(white: 0.5, alpha: 1.0)

        let separator = UIView()
        separator.backgroundColor = UIColor(white: 0.3, alpha: 1.0)
        separator.translatesAutoresizingMaskIntoConstraints = false
        separator.heightAnchor.constraint(equalToConstant: 1).isActive = true

        stack.addArrangedSubview(titleLabel)
        stack.addArrangedSubview(separator)
        separator.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
        for label in labels { stack.addArrangedSubview(label) }

        labelsContainer.addSubview(stack)
        NSLayoutConstraint.activate([
            labelsContainer.topAnchor.constraint(equalTo: view.topAnchor),
            labelsContainer.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            labelsContainer.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            labelsContainer.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 20),
            stack.leadingAnchor.constraint(equalTo: labelsContainer.leadingAnchor, constant: 20),
            stack.trailingAnchor.constraint(equalTo: labelsContainer.trailingAnchor, constant: -20)
        ])

        // --- Metrics overlay (floating, always on top) ---
        metricsOverlay.translatesAutoresizingMaskIntoConstraints = false
        metricsOverlay.isHidden = true
        view.addSubview(metricsOverlay)

        NSLayoutConstraint.activate([
            metricsOverlay.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 8),
            metricsOverlay.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -8),
        ])

        updateUILabels()
    }

    /// Switch to realistic rendering mode (full-screen surface + floating overlay).
    private func enterRealisticMode() {
        labelsContainer.isHidden = true
        surfaceScrollView.isHidden = false
        metricsOverlay.isHidden = false
    }

    /// Switch back to stress labels mode.
    private func enterStressMode() {
        labelsContainer.isHidden = false
        surfaceScrollView.isHidden = true
        metricsOverlay.isHidden = true
    }

    // MARK: - Surface View Management

    private func addSurfaceView(_ surface: Surface) {
        let surfaceView = surface.view
        surfaceView.translatesAutoresizingMaskIntoConstraints = false
        surfaceContentView.addSubview(surfaceView)

        // Position surface views vertically stacked
        relayoutSurfaceViews()

        surface.onLayoutChanged = { [weak self] in
            self?.relayoutSurfaceViews()
        }
    }

    private func removeSurfaceView(_ surface: Surface) {
        surface.view.removeFromSuperview()
        relayoutSurfaceViews()
    }

    private func relayoutSurfaceViews() {
        var yOffset: CGFloat = 0
        let width = view.bounds.width
        for subview in surfaceContentView.subviews {
            subview.frame = CGRect(x: 0, y: yOffset, width: width, height: subview.frame.height)
            yOffset += subview.frame.height
        }
        surfaceContentView.frame = CGRect(x: 0, y: 0, width: width, height: yOffset)
        surfaceScrollView.contentSize = CGSize(width: width, height: yOffset)
    }

    private func updateUILabels() {
        let scenarioName = config.scenario.rawValue
        scenarioLabel.text = "Scenario: \(scenarioName)"

        let roundStr = config.maxRounds > 0 ? "\(currentRound) / \(config.maxRounds)" : "\(currentRound)"
        roundLabel.text = "Round: \(roundStr)"

        let elapsed = startTime != nil ? Date().timeIntervalSince(startTime) : 0
        let h = Int(elapsed) / 3600
        let m = (Int(elapsed) % 3600) / 60
        let s = Int(elapsed) % 60
        durationLabel.text = String(format: "Duration: %02d:%02d:%02d", h, m, s)

        memoryMonitor.update()
        memoryLabel.text = "Memory: \(memoryMonitor.getSummary())"

        statusLabel.text = isRunning ? "Status: RUNNING" : "Status: STOPPED"
        statusLabel.textColor = isRunning ? .systemGreen : .systemOrange

        lastRoundLabel.text = lastRoundInfo.isEmpty ? "Last: -" : lastRoundInfo
        errorLabel.text = "Errors: \(errorCount)"
        logPathLabel.text = "Log: \(logger.logFilePath)"
    }

    private func updateMetricsOverlay() {
        let elapsed = startTime != nil ? Date().timeIntervalSince(startTime) : 0
        memoryMonitor.update()
        metricsOverlay.update(
            scenario: lastRoundInfo,
            round: currentRound,
            maxRounds: config.maxRounds,
            elapsed: elapsed,
            memoryMb: memoryMonitor.getCurrentMb(),
            peakMb: memoryMonitor.peakMb,
            errors: errorCount,
            isRunning: isRunning
        )
    }

    // MARK: - Test Loop

    private func startTest() {
        startTime = Date()
        isRunning = true

        let detail = "scenario=\(config.scenario.rawValue),duration_min=\(config.durationMinutes),max_rounds=\(config.maxRounds),interval_ms=\(config.intervalMs)"
        logger.logEvent(event: "start", detail: detail)

        if let reason = config.restartReason {
            logger.logEvent(event: "restart", detail: "reason=\(reason)")
        }

        // Start periodic UI update timer for overlay
        uiUpdateTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.updateMetricsOverlay()
            self?.updateUILabels()
        }

        scheduleNextRound()
    }

    private func scheduleNextRound() {
        let delay = DispatchTimeInterval.milliseconds(config.intervalMs)
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            self?.runNextRound()
        }
    }

    private func runNextRound() {
        guard isRunning, !isRealisticRoundInProgress else { return }

        // Check termination: duration
        let elapsedMs = Int(Date().timeIntervalSince(startTime) * 1000)
        let durationMs = config.durationMinutes * 60 * 1000
        if elapsedMs >= durationMs {
            stopTest(reason: "Duration limit reached (\(config.durationMinutes) min)")
            return
        }

        // Check termination: max rounds
        if config.maxRounds > 0 && currentRound >= config.maxRounds {
            stopTest(reason: "Max rounds reached (\(config.maxRounds))")
            return
        }

        currentRound += 1

        // Determine effective scenario
        var effectiveScenario = config.scenario
        var subScenario: String? = nil

        if config.scenario.isMeta {
            guard let selected = engine.selectCombinedScenario(mode: config.scenario, crashTracker: crashTracker) else {
                stopTest(reason: "All scenarios blacklisted")
                return
            }
            effectiveScenario = selected
            subScenario = selected.rawValue
        } else if crashTracker.isBlacklisted(config.scenario.rawValue) {
            stopTest(reason: "Scenario \(config.scenario.rawValue) blacklisted")
            return
        }

        // Write-ahead crash state
        crashTracker.beforeRound(scenario: config.scenario.rawValue,
                                 subScenario: subScenario,
                                 round: currentRound,
                                 fixture: nil)

        if effectiveScenario.isRealistic {
            executeRealisticRound(effectiveScenario)
        } else {
            executeStressRound(effectiveScenario)
        }
    }

    // MARK: - Stress Round (synchronous)

    private func executeStressRound(_ scenario: StabilityScenario) {
        enterStressMode()

        let roundStart = CFAbsoluteTimeGetCurrent()
        let result: ScenarioRoundResult = autoreleasepool {
            return engine.executeRound(scenario)
        }
        let durationRoundMs = Int((CFAbsoluteTimeGetCurrent() - roundStart) * 1000)

        completeRound(scenario: scenario, result: result, durationMs: durationRoundMs)
    }

    // MARK: - Realistic Round (asynchronous)

    private func executeRealisticRound(_ scenario: StabilityScenario) {
        enterRealisticMode()
        isRealisticRoundInProgress = true

        // Clear previous surface views
        surfaceContentView.subviews.forEach { $0.removeFromSuperview() }
        surfaceScrollView.contentSize = .zero

        let roundStart = CFAbsoluteTimeGetCurrent()

        realisticEngine.executeRound(scenario, containerWidth: view.bounds.width) { [weak self] result in
            guard let self = self else { return }
            let durationRoundMs = Int((CFAbsoluteTimeGetCurrent() - roundStart) * 1000)
            self.isRealisticRoundInProgress = false
            self.completeRound(scenario: scenario, result: result, durationMs: durationRoundMs)
        }
    }

    // MARK: - Round Completion

    private func completeRound(scenario: StabilityScenario, result: ScenarioRoundResult, durationMs: Int) {
        crashTracker.afterRound()

        let status = result.isSuccess ? "ok" : "error"
        if !result.isSuccess { errorCount += 1 }

        memoryMonitor.update()
        logger.logRound(round: currentRound, scenario: scenario.rawValue,
                       durationMs: durationMs, status: status,
                       fixture: result.fixture, error: result.error,
                       memoryMb: memoryMonitor.getCurrentMb())

        let scenarioShort = String(scenario.rawValue.prefix(16))
        lastRoundInfo = "R\(currentRound) \(scenarioShort) \(status) (\(durationMs)ms)"
        if let fix = result.fixture {
            lastRoundInfo += " [\(fix)]"
        }
        updateUILabels()
        updateMetricsOverlay()

        // Schedule next round
        scheduleNextRound()
    }

    private func stopTest(reason: String) {
        isRunning = false
        uiUpdateTimer?.invalidate()
        uiUpdateTimer = nil
        realisticEngine.teardown()
        crashTracker.markCleanExit()
        logger.logEvent(event: "stop", detail: reason)
        logger.close()
        writeDoneMarker(reason: reason)

        lastRoundInfo = "STOPPED: \(reason)"
        updateUILabels()
        updateMetricsOverlay()
    }

    private func writeDoneMarker(reason: String) {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let stabilityDir = docs.appendingPathComponent("stability")
        let doneFileURL = stabilityDir.appendingPathComponent("stability_done.txt")
        let content = "\(reason)\n\(Date().timeIntervalSince1970)\n"
        try? content.write(to: doneFileURL, atomically: true, encoding: .utf8)
    }
}
