import XCTest
@testable import Playground
@testable import AGenUI

/// RISK49: DateTimeInputComponent popup views permanently leaked on key window
/// when component is deallocated while popup is visible.
///
/// Root cause (iOS-specific):
///   1. showPopup() adds customMaskView and popupContainerView directly to the
///      key window (UIApplication.shared.windows.first(where: { $0.isKeyWindow })).
///   2. hidePopup() removes them.
///   3. DateTimeInputComponent has NO deinit — there is no cleanup path.
///   4. When the component is deallocated while the popup is visible (e.g., the
///      surface is destroyed during date selection), the mask and popup container
///      views remain on the key window permanently.
///
/// Additionally, the mask view's UITapGestureRecognizer targets `self`
/// (DateTimeInputComponent). Depending on whether UIGestureRecognizer retains
/// its target:
///   a. If retained: the component itself is leaked (retain cycle via key window).
///   b. If not retained: tapping the orphaned mask sends a message to a
///      deallocated object → EXC_BAD_ACCESS.
///
/// Attack surface:
///   Any A2UI card containing a DateTimeInput component. If the user opens the
///   date picker popup and the surface is destroyed mid-interaction (e.g., new
///   streaming response replaces the card, or host app navigates away), the
///   popup overlay permanently covers the key window — a visual artifact and
///   memory leak that compounds across sessions.
///
/// Severity: HIGH — permanent view leak on key window, potential crash on
///   interaction with orphaned overlay, accumulates over app lifetime.
/// Fix: Add deinit that calls hidePopup(), or use [weak self] in the mask
///   gesture recognizer and guard against nil.
@MainActor
final class SDKRiskProbeDateTimeInputPopupLeakTest: XCTestCase {

    // MARK: - RISK49a: Popup views leaked on key window after component deallocation

    /// Create a DateTimeInputComponent, show its popup, then deallocate the
    /// component without calling hidePopup. The mask and popup container views
    /// should be removed from the key window, but without deinit cleanup they
    /// will remain — confirming the leak.
    func testRISK49a_popupViewsLeakOnKeyWindowAfterDealloc() {
        // Disable animations so showPopup/hidePopup complete synchronously
        UIView.setAnimationsEnabled(false)
        defer { UIView.setAnimationsEnabled(true) }

        guard let keyWindow = UIApplication.shared.windows.first(where: { $0.isKeyWindow }) else {
            XCTFail("No key window available for test — cannot test popup leak")
            return
        }

        let initialSubviewCount = keyWindow.subviews.count
        weak var weakComponent: Component?

        autoreleasepool {
            let component = DateTimeInputComponent(componentId: "dt_risk49a", properties: [
                "enableDate": true,
                "enableTime": false
            ])
            weakComponent = component

            // Attach to key window so component is in a live view hierarchy
            keyWindow.addSubview(component)
            component.frame = CGRect(x: 0, y: 100, width: 200, height: 56)

            // Trigger popup by tapping the compact button
            let buttons = component.subviews.compactMap { $0 as? UIButton }
            guard let compactButton = buttons.first else {
                XCTFail("Could not find compact button in DateTimeInputComponent subviews")
                component.removeFromSuperview()
                return
            }
            compactButton.sendActions(for: .touchUpInside)

            // Verify popup was shown — key window should have gained mask + popup views
            let afterPopupCount = keyWindow.subviews.count
            // Expected: initial + 1 (component itself) + 2 (mask + popup container)
            XCTAssertGreaterThanOrEqual(afterPopupCount, initialSubviewCount + 3,
                                       "Popup mask and container should be added to key window")

            // Now remove component (simulating Surface.removeComponent)
            component.removeFromSuperview()
            // `component` goes out of scope here → should be deallocated
        }

        // Allow pending autoreleases to drain
        RunLoop.current.run(until: Date(timeIntervalSinceNow: 0.1))

        let finalSubviewCount = keyWindow.subviews.count
        let leakedViews = finalSubviewCount - initialSubviewCount
        let componentAlive = weakComponent != nil

        // Clean up any leaked views for test hygiene
        if leakedViews > 0 {
            let leakedSubviews = Array(keyWindow.subviews.suffix(leakedViews))
            for view in leakedSubviews {
                view.removeFromSuperview()
            }
        }

        if leakedViews > 0 || componentAlive {
            XCTFail("RISK49a: CONFIRMED — DateTimeInputComponent popup views leaked on key window. "
                    + "Leaked views: \(leakedViews), Component still alive: \(componentAlive). "
                    + "Root cause: No deinit to call hidePopup(). Mask and popup container views "
                    + "added to key window in showPopup() are never removed when the component "
                    + "is deallocated while popup is visible.")
        }
    }

    // MARK: - RISK49b: Control — normal hide-then-dealloc cleans up correctly

    /// Show popup, then hide it via button toggle, then dealloc.
    /// Expected: all views removed normally, no leak.
    func testRISK49b_controlGroup_normalHideThenDeallocCleansUp() {
        UIView.setAnimationsEnabled(false)
        defer { UIView.setAnimationsEnabled(true) }

        guard let keyWindow = UIApplication.shared.windows.first(where: { $0.isKeyWindow }) else {
            XCTFail("No key window available for test")
            return
        }

        let initialSubviewCount = keyWindow.subviews.count
        weak var weakComponent: Component?

        autoreleasepool {
            let component = DateTimeInputComponent(componentId: "dt_control", properties: [
                "enableDate": true,
                "enableTime": false
            ])
            weakComponent = component

            keyWindow.addSubview(component)
            component.frame = CGRect(x: 0, y: 100, width: 200, height: 56)

            // Show popup
            let buttons = component.subviews.compactMap { $0 as? UIButton }
            guard let compactButton = buttons.first else {
                XCTFail("Could not find compact button")
                component.removeFromSuperview()
                return
            }
            compactButton.sendActions(for: .touchUpInside)

            // Hide popup (toggle)
            compactButton.sendActions(for: .touchUpInside)

            // Remove component
            component.removeFromSuperview()
        }

        RunLoop.current.run(until: Date(timeIntervalSinceNow: 0.1))

        let finalSubviewCount = keyWindow.subviews.count
        let componentAlive = weakComponent != nil

        // Control: everything should be cleaned up
        XCTAssertEqual(finalSubviewCount, initialSubviewCount,
                      "Control: Normal popup hide should clean up all views from key window. "
                      + "Initial: \(initialSubviewCount), Final: \(finalSubviewCount)")
        // Note: component may still be alive due to hidePopup's strong self capture
        // in the animation completion, but with animations disabled this should drain.
    }
}
