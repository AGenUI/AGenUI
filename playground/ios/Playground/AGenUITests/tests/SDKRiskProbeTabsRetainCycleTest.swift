import XCTest
@testable import Playground
@testable import AGenUI

/// RISK48: TabsComponent.addChild creates an unbreakable retain cycle on every child.
///
/// Root cause (iOS-specific):
///   In TabsComponent.addChild(_:), the code sets:
///     child.onPropertiesUpdate = { [weak self] _ in
///         ...
///         child.frame = childFrame   // `child` captured STRONGLY
///         ...
///     }
///   The closure is stored on `child.onPropertiesUpdate` (owned by child).
///   The closure also captures `child` strongly.
///   This forms: child → closure → child (retain cycle).
///
///   Even when TabsComponent is deallocated and its `children` array is released,
///   the child retains itself through its own closure — it can NEVER be freed.
///
/// Attack surface:
///   Every child added to a TabsComponent is permanently leaked.
///   In a long-running chat app (e.g., streaming A2UI cards with Tabs), each surface
///   refresh creates new child trees that are never freed → unbounded memory growth → OOM.
///
/// Severity: HIGH — deterministic permanent memory leak, compounds over time to OOM crash.
/// Fix: Use [weak child] capture in the closure, or nil out onPropertiesUpdate on removal.
@MainActor
final class SDKRiskProbeTabsRetainCycleTest: XCTestCase {

    // MARK: - RISK48a: Prove children are leaked after TabsComponent deallocation

    /// Round 1: Create a TabsComponent, add children, release TabsComponent.
    /// If retain cycle exists: children remain alive (weak ref non-nil).
    /// If no retain cycle: children are deallocated (weak ref becomes nil).
    func testRISK48a_tabsChildRetainCycle() {
        weak var weakChild1: Component?
        weak var weakChild2: Component?

        autoreleasepool {
            let tabs = TabsComponent(componentId: "tabs1", properties: [
                "tabs": [
                    ["title": "Tab A", "child": "child1"],
                    ["title": "Tab B", "child": "child2"]
                ]
            ])

            let child1 = ColumnComponent(componentId: "child1", properties: [:])
            let child2 = ColumnComponent(componentId: "child2", properties: [:])

            tabs.addChild(child1)
            tabs.addChild(child2)

            weakChild1 = child1
            weakChild2 = child2

            // At this point:
            // - tabs.children holds strong refs to child1, child2
            // - child1.onPropertiesUpdate closure captures child1 strongly (retain cycle)
            // - child2.onPropertiesUpdate closure captures child2 strongly (retain cycle)

            // Verify children were added
            XCTAssertEqual(tabs.children.count, 2, "Children should be added to tabs")
        }
        // After autoreleasepool: tabs is deallocated, children array released.
        // If retain cycle exists, children survive because they retain themselves.

        if weakChild1 != nil || weakChild2 != nil {
            XCTFail("RISK48a: CONFIRMED — TabsComponent child memory leak detected. " +
                    "child1 alive: \(weakChild1 != nil), child2 alive: \(weakChild2 != nil). " +
                    "Children retain themselves via onPropertiesUpdate closure capture. " +
                    "Every TabsComponent usage leaks all child components permanently. " +
                    "In production: unbounded memory growth → OOM crash.")
        }
    }

    // MARK: - RISK48b: Prove leak persists even after removeChild

    /// Round 2: Add children, then explicitly remove them via removeChild.
    /// If the closure is not cleared on removal, children still leak.
    func testRISK48b_tabsChildLeakAfterRemoval() {
        weak var weakChild: Component?

        autoreleasepool {
            let tabs = TabsComponent(componentId: "tabs2", properties: [
                "tabs": [
                    ["title": "Tab A", "child": "childA"]
                ]
            ])

            let child = ColumnComponent(componentId: "childA", properties: [:])
            tabs.addChild(child)
            weakChild = child

            // Explicitly remove the child
            tabs.removeChild(child)

            // Verify child is removed from tree
            XCTAssertEqual(tabs.children.count, 0, "Child should be removed from tabs.children")
        }
        // After autoreleasepool: tabs gone, child removed.
        // If onPropertiesUpdate closure is NOT cleared by removeChild,
        // child still holds itself via retain cycle.

        if weakChild != nil {
            XCTFail("RISK48b: CONFIRMED — Child component leaked even after removeChild(). " +
                    "removeChild does NOT clear child.onPropertiesUpdate, " +
                    "so the self-referencing closure keeps the child alive forever. " +
                    "Memory leak persists regardless of component tree management.")
        }
    }

    // MARK: - RISK48c: Control — Component without TabsComponent closure is freed

    /// Control: A Component NOT added to TabsComponent should be properly freed.
    /// This proves the leak is specifically caused by TabsComponent.addChild's closure.
    func testRISK48c_controlNoRetainCycleWithoutTabs() {
        weak var weakChild: Component?

        autoreleasepool {
            // Declare "child" in parent's children list so addChild accepts it
            let parent = ColumnComponent(componentId: "parent", properties: [
                "children": ["child"]
            ])
            let child = ColumnComponent(componentId: "child", properties: [:])
            parent.addChild(child)
            weakChild = child

            // Verify child was added
            XCTAssertEqual(parent.children.count, 1, "Child should be added")
        }
        // After autoreleasepool: parent and child should both be freed
        // because ColumnComponent.addChild does NOT set a self-referencing closure.

        XCTAssertNil(weakChild,
                     "RISK48c: Control FAILED — child should be deallocated when parent is freed. " +
                     "If this fails, there's a broader retain cycle in Component base class.")
    }
}
