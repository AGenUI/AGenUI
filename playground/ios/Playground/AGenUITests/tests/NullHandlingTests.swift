//
//  NullHandlingTests.swift
//  AGenUITests
//
//  TDD tests for AGenUI null handling alignment (iOS Swift layer).
//
//  Tests the DiffValue-based delete signal path:
//  - DiffValue enum pattern matching
//  - Surface NSNull → DiffValue conversion
//  - Component.updateProperties([String: DiffValue]) .deleted handling
//  - TextComponent leaf: text=.deleted clears to ""
//  - Component base: action=.deleted removes tap gesture
//  - Component base: accessibility=.deleted resets to system default
//  - CSSPropertyApplier reads from styles sub-dictionary (not flattened)
//

import XCTest
@testable import Playground
@testable import AGenUI

@MainActor
class NullHandlingTests: AGenUIBaseTest {

    // MARK: - DiffValue enum

    func testDiffValue_value_carriesPayload() {
        let dv = DiffValue.value("hello")
        if case .value(let v) = dv {
            XCTAssertEqual(v as? String, "hello")
        } else {
            XCTFail("Expected .value case")
        }
    }

    func testDiffValue_deleted_matchesDirectly() {
        let dv: DiffValue = .deleted
        if case .deleted = dv {
            // pass
        } else {
            XCTFail("Expected .deleted case")
        }
    }

    func testDiffValue_from_convertsNSNullToDeleted() {
        let raw: [String: Any] = ["text": "hello", "url": NSNull(), "count": 42]
        let diff = DiffValue.from(raw)
        if case .value(let v) = diff["text"] {
            XCTAssertEqual(v as? String, "hello")
        } else {
            XCTFail("text should be .value")
        }
        if case .deleted = diff["url"] {
            // pass
        } else {
            XCTFail("url should be .deleted")
        }
        if case .value(let v) = diff["count"] {
            XCTAssertEqual(v as? Int, 42)
        } else {
            XCTFail("count should be .value")
        }
    }

    // MARK: - TextComponent leaf: text=.deleted clears to ""

    func testTextComponent_deletedText_clearsLabel() {
        let component = TextComponent(componentId: "t1", properties: [:])
        // Set initial text
        component.updateProperties(["text": .value("Hello World")])
        let label = component.subviews.compactMap { $0 as? UILabel }.first
        XCTAssertEqual(label?.text, "Hello World")

        // Send text=null (delete signal)
        component.updateProperties(["text": .deleted])
        // null = clear to type empty value (string → "")
        let cleared = label?.text ?? "<nil>"
        XCTAssertTrue(cleared.trimmingCharacters(in: .whitespaces).isEmpty,
                       "text=.deleted should clear label, got: \(cleared)")
    }

    func testTextComponent_valueText_updatesLabel() {
        let component = TextComponent(componentId: "t2", properties: [:])
        component.updateProperties(["text": .value("First")])
        let label = component.subviews.compactMap { $0 as? UILabel }.first
        XCTAssertEqual(label?.text, "First")

        component.updateProperties(["text": .value("Second")])
        XCTAssertEqual(label?.text, "Second")
    }

    // MARK: - Component base: action=.deleted removes tap gesture

    func testComponent_deletedAction_removesTapGesture() {
        let component = TextComponent(componentId: "a1", properties: [:])
        let action: [String: Any] = ["type": "tap", "url": "app://home"]
        component.updateProperties(["action": .value(action)])
        // After setting action, there should be a tap gesture
        let hasGestureBefore = component.gestureRecognizers?.contains(where: { $0 is UITapGestureRecognizer }) ?? false
        XCTAssertTrue(hasGestureBefore, "Should have tap gesture after action set")

        // Send action=null (delete signal)
        component.updateProperties(["action": .deleted])
        let hasGestureAfter = component.gestureRecognizers?.contains(where: { $0 is UITapGestureRecognizer }) ?? false
        XCTAssertFalse(hasGestureAfter, "Tap gesture should be removed after action=.deleted")
    }

    // MARK: - Component base: accessibility=.deleted resets to system default

    func testComponent_deletedAccessibility_resetsToDefault() {
        let component = TextComponent(componentId: "a11y1", properties: [:])
        let a11y: [String: Any] = ["label": "Save Button", "description": "Saves your work"]
        component.updateProperties(["accessibility": .value(a11y)])
        XCTAssertTrue(component.isAccessibilityElement, "Should be accessibility element after setting a11y")
        XCTAssertEqual(component.accessibilityLabel, "Save Button")

        // Send accessibility=null (delete signal)
        component.updateProperties(["accessibility": .deleted])
        XCTAssertFalse(component.isAccessibilityElement, "Should reset to non-accessibility element")
        XCTAssertNil(component.accessibilityLabel, "Label should be nil after reset")
    }

    // MARK: - Surface boundary: NSNull → DiffValue conversion

    func testSurface_updateComponent_withNSNull_reachesComponentAsDeleted() {
        let surface = Surface(surfaceId: "surf-null-test")

        // Add a Text component with initial text
        surface.addComponent(
            componentId: "root",
            componentType: "Text",
            properties: ["text": "Original", "styles": ["x": 0, "y": 0, "width": 200, "height": 40]],
            parentId: nil
        )

        guard let component = surface.getComponent(componentId: "root") as? TextComponent else {
            XCTFail("Text component not found")
            return
        }
        let label = component.subviews.compactMap { $0 as? UILabel }.first
        XCTAssertEqual(label?.text, "Original")

        // Update with text=null (NSNull) — Surface must convert to DiffValue.deleted
        surface.updateComponent(componentId: "root", properties: ["text": NSNull()])

        // The text should be cleared, not retained
        let cleared = label?.text ?? "<nil>"
        XCTAssertTrue(cleared.trimmingCharacters(in: .whitespaces).isEmpty,
                       "Surface should pass NSNull as .deleted, clearing text. Got: \(cleared)")
    }

    // MARK: - CSSPropertyApplier: styles kept at second level

    func testCSSPropertyApplier_readsFromStylesSubDictionary() {
        let component = TextComponent(componentId: "css1", properties: [:])
        // styles includes background-color — CSSPropertyApplier should read it from styles
        let styles: [String: Any] = [
            "x": 0, "y": 0, "width": 100, "height": 50,
            "background-color": "#FF0000"
        ]
        component.updateProperties([
            "styles": .value(styles)
        ])
        // background-color should be applied to the component's background
        let expectedBg = UIColor(hexString: "#FF0000")
        XCTAssertEqual(component.backgroundColor, expectedBg,
                       "background-color should be applied from styles sub-dictionary")
    }
}
