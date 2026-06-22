import Foundation
import UIKit
import AGenUI

/// Async engine that executes realistic scenarios with visible UI rendering.
/// Each scenario loads a fixture JSON with timed steps, renders surfaces full-screen,
/// and reports results via completion handler.
class RealisticScenarioEngine: NSObject, SurfaceManagerListener {

    // MARK: - Types

    private struct FixtureStep {
        let action: String
        let delayMs: Int
        let message: String?   // serialized JSON for normal actions
        let raw: String?       // raw text for error injection
    }

    private struct Fixture {
        let description: String
        let surfaceId: String
        let isMultiSurface: Bool
        let steps: [FixtureStep]
    }

    // MARK: - Callbacks

    /// Called when a surface view is created and should be added to the view hierarchy.
    var onSurfaceCreated: ((Surface) -> Void)?

    /// Called when a surface is deleted and its view should be removed.
    var onSurfaceDeleted: ((Surface) -> Void)?

    /// Called when all steps have finished for the current round.
    private var roundCompletion: ((ScenarioRoundResult) -> Void)?

    // MARK: - State

    private var surfaceManager: SurfaceManager?
    private var activeSurfaces: [String: Surface] = [:]
    private var isExecuting = false
    private var currentFixtureName: String?
    private var stepIndex = 0
    private var currentSteps: [FixtureStep] = []
    private var executionCancelled = false

    // MARK: - Fixture Mapping

    private static let scenarioFixtureMap: [StabilityScenario: String] = [
        .realisticArticleStream: "article_stream",
        .realisticMultiCard: "multi_card",
        .realisticFormFill: "form_fill",
        .realisticChartRefresh: "chart_refresh",
        .realisticLongList: "long_list",
        .realisticPageSwitch: "page_switch",
        .realisticTabNavigation: "tab_navigation",
        .realisticLottieCarousel: "lottie_carousel",
        .realisticMixedDashboard: "mixed_dashboard",
        .realisticErrorRecovery: "error_recovery",
    ]

    // MARK: - Public API

    /// Execute a realistic scenario round asynchronously.
    /// Must be called on the main thread.
    /// - Parameters:
    ///   - scenario: The realistic scenario to execute.
    ///   - containerWidth: Width constraint for surface rendering.
    ///   - completion: Called on the main thread when the round finishes.
    func executeRound(_ scenario: StabilityScenario, containerWidth: CGFloat, completion: @escaping (ScenarioRoundResult) -> Void) {
        guard !isExecuting else {
            completion(ScenarioRoundResult(fixture: nil, error: "Engine busy"))
            return
        }
        guard scenario.isRealistic else {
            completion(ScenarioRoundResult(fixture: nil, error: "Not a realistic scenario"))
            return
        }

        guard let fixtureName = Self.scenarioFixtureMap[scenario] else {
            completion(ScenarioRoundResult(fixture: nil, error: "No fixture for \(scenario.rawValue)"))
            return
        }

        guard let fixture = loadFixture(name: fixtureName) else {
            completion(ScenarioRoundResult(fixture: fixtureName, error: "Failed to load fixture"))
            return
        }

        isExecuting = true
        executionCancelled = false
        currentFixtureName = fixtureName
        currentSteps = fixture.steps
        stepIndex = 0
        roundCompletion = completion

        // Create a fresh SurfaceManager for this round
        let sm = SurfaceManager()
        sm.addListener(self)
        surfaceManager = sm
        sm.beginTextStream()

        // Start executing steps
        executeNextStep(containerWidth: containerWidth)
    }

    /// Cancel current execution. The completion handler will be called with a cancellation error.
    func cancel() {
        guard isExecuting else { return }
        executionCancelled = true
        finishRound(error: "Cancelled")
    }

    /// Tear down any in-progress state (e.g., when the VC disappears).
    func teardown() {
        executionCancelled = true
        cleanupSurfaces()
        surfaceManager?.endTextStream()
        surfaceManager?.removeAllListeners()
        surfaceManager = nil
        isExecuting = false
        roundCompletion = nil
    }

    // MARK: - Step Execution

    private func executeNextStep(containerWidth: CGFloat) {
        guard !executionCancelled else { return }
        guard stepIndex < currentSteps.count else {
            // All steps done — end stream and finish
            surfaceManager?.endTextStream()
            // Allow a brief delay for final layout to settle
            DispatchQueue.main.asyncAfter(deadline: .now() + .milliseconds(200)) { [weak self] in
                self?.finishRound(error: nil)
            }
            return
        }

        let step = currentSteps[stepIndex]
        stepIndex += 1

        let delay = DispatchTimeInterval.milliseconds(max(step.delayMs, 0))
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            guard let self = self, !self.executionCancelled else { return }
            self.performStep(step, containerWidth: containerWidth)
            self.executeNextStep(containerWidth: containerWidth)
        }
    }

    private func performStep(_ step: FixtureStep, containerWidth: CGFloat) {
        guard let sm = surfaceManager else { return }

        switch step.action {
        case "createSurface", "updateComponents", "updateDataModel", "deleteSurface":
            if let message = step.message {
                sm.receiveTextChunk(message)
            }

        case "rawChunk":
            // Inject raw (potentially malformed) data
            if let raw = step.raw {
                sm.receiveTextChunk(raw)
            }

        case "beginNewStream":
            // Simulate interrupt: end current stream, begin a new one
            sm.endTextStream()
            sm.beginTextStream()

        default:
            // Unknown action — treat message as raw chunk if available
            if let message = step.message {
                sm.receiveTextChunk(message)
            }
        }
    }

    // MARK: - SurfaceManagerListener

    func onCreateSurface(_ surface: Surface) {
        let screenWidth = UIScreen.main.bounds.width
        surface.updateSize(width: screenWidth, height: .infinity)
        activeSurfaces[surface.surfaceId] = surface

        surface.onLayoutChanged = { [weak surface] in
            guard let s = surface else { return }
            // Update view frame for scroll sizing
            let height = s.view.frame.size.height
            if height > 0 {
                s.view.frame = CGRect(x: 0, y: s.view.frame.origin.y,
                                      width: screenWidth, height: height)
            }
        }

        onSurfaceCreated?(surface)
    }

    func onDeleteSurface(_ surface: Surface) {
        activeSurfaces.removeValue(forKey: surface.surfaceId)
        onSurfaceDeleted?(surface)
    }

    // MARK: - Finish & Cleanup

    private func finishRound(error: String?) {
        cleanupSurfaces()
        surfaceManager?.removeAllListeners()
        surfaceManager = nil
        isExecuting = false

        let result = ScenarioRoundResult(fixture: currentFixtureName, error: error)
        let completion = roundCompletion
        roundCompletion = nil
        currentFixtureName = nil
        currentSteps = []
        stepIndex = 0

        completion?(result)
    }

    private func cleanupSurfaces() {
        for (_, surface) in activeSurfaces {
            onSurfaceDeleted?(surface)
        }
        activeSurfaces.removeAll()
    }

    // MARK: - Fixture Loading

    private func loadFixture(name: String) -> Fixture? {
        guard let path = Bundle.main.path(forResource: name, ofType: "json",
                                          inDirectory: "stability_fixtures/realistic_scenarios"),
              let data = FileManager.default.contents(atPath: path) else {
            return nil
        }

        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }

        let description = json["description"] as? String ?? name
        let surfaceId = json["surfaceId"] as? String ?? "realistic-\(name)"
        let isMultiSurface = json["multi_surface"] as? Bool ?? false

        guard let stepsArray = json["steps"] as? [[String: Any]] else {
            return nil
        }

        var steps: [FixtureStep] = []
        for stepDict in stepsArray {
            let action = stepDict["action"] as? String ?? "unknown"
            let delayMs = stepDict["delay_ms"] as? Int ?? 0

            var messageStr: String?
            if let messageObj = stepDict["message"] {
                if let msgData = try? JSONSerialization.data(withJSONObject: messageObj),
                   let msgStr = String(data: msgData, encoding: .utf8) {
                    messageStr = msgStr
                }
            }

            let raw = stepDict["raw"] as? String

            steps.append(FixtureStep(action: action, delayMs: delayMs, message: messageStr, raw: raw))
        }

        return Fixture(description: description, surfaceId: surfaceId,
                       isMultiSurface: isMultiSurface, steps: steps)
    }
}
