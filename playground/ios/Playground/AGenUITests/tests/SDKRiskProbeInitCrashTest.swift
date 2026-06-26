import XCTest
@testable import AGenUI

final class SDKRiskProbeInitCrashTest: XCTestCase {

    private let createCount = 200

    func testSDKRISK01_surfaceManagerInitLoop() {
        var managers: [SurfaceManager] = []
        managers.reserveCapacity(createCount)

        for i in 0..<createCount {
            autoreleasepool {
                let manager = SurfaceManager()
                managers.append(manager)

                manager.beginTextStream()
                manager.receiveTextChunk(Self.createSurfaceJSON(surfaceId: "sdk-risk-ios-\(i)"))
                manager.endTextStream()

                if i % 10 == 0 {
                    AGenUISDK.setDayNightMode(i % 20 == 0 ? "light" : "dark")
                }
            }
        }

        let drain = expectation(description: "drain main queue")
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            drain.fulfill()
        }
        wait(for: [drain], timeout: 5.0)

        XCTAssertEqual(managers.count, createCount)
        _ = managers.map { $0.getInstanceId() }
        managers.removeAll()
    }

    private static func createSurfaceJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}
        """
    }
}
