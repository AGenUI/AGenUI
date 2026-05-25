//
//  UITestViewController.swift
//  Playground
//
//  Lightweight test rendering entry for automated UI testing.
//  Launched via:
//    xcrun simctl launch booted <bundle_id> --testBundleDataB64 <base64>
//    xcrun simctl launch booted <bundle_id> --testCase <case_id>
//

import UIKit
import AGenUI

class UITestViewController: UIViewController, SurfaceManagerListener {
    private let surfaceManager = SurfaceManager()
    private let scrollView = UIScrollView()
    private let testDataPath: String?
    private let bundleJsonString: String?

    init(testDataPath: String) {
        self.testDataPath = testDataPath
        self.bundleJsonString = nil
        super.init(nibName: nil, bundle: nil)
    }

    init(bundleJsonString: String) {
        self.bundleJsonString = bundleJsonString
        self.testDataPath = nil
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white

        scrollView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scrollView)
        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
        ])

        surfaceManager.addListener(self)

        if let jsonStr = bundleJsonString {
            renderFromData(jsonStr)
        } else if let path = testDataPath {
            loadAndRender(path)
        }
    }

    private func renderFromData(_ jsonStr: String) {
        guard let data = jsonStr.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let sequence = json["sequence"] as? [[String: Any]],
              let version = json["version"] as? String else {
            print("[UITest] Failed to parse inline JSON data")
            return
        }

        surfaceManager.beginTextStream()
        for item in sequence {
            var msg = item
            msg["version"] = version
            if let msgData = try? JSONSerialization.data(withJSONObject: msg),
               let msgStr = String(data: msgData, encoding: .utf8) {
                surfaceManager.receiveTextChunk(msgStr)
            }
        }
        surfaceManager.endTextStream()

        print("[UITest] Rendered \(sequence.count) messages from inline data")
    }

    private func loadAndRender(_ path: String) {
        let fullPath: String
        if path.hasPrefix("/") {
            fullPath = path
        } else {
            fullPath = NSTemporaryDirectory() + path
        }

        guard let data = FileManager.default.contents(atPath: fullPath),
              let content = String(data: data, encoding: .utf8) else {
            print("[UITest] Failed to load test data from: \(fullPath)")
            return
        }

        renderFromData(content)
        print("[UITest] Loaded from file: \(fullPath)")
    }

    // MARK: - SurfaceManagerListener
    func onCreateSurface(_ surface: Surface) {
        scrollView.subviews.forEach { $0.removeFromSuperview() }
        surface.updateSize(width: view.bounds.width, height: .infinity)
        scrollView.addSubview(surface.view)
        surface.onLayoutChanged = { [weak self] in
            guard let self = self else { return }
            let contentHeight = surface.view.frame.height
            let scrollHeight = self.scrollView.frame.height
            if contentHeight < scrollHeight {
                self.scrollView.contentInset.top = (scrollHeight - contentHeight) / 2
            } else {
                self.scrollView.contentInset.top = 0
            }
            self.scrollView.contentSize = CGSize(
                width: self.scrollView.frame.width,
                height: contentHeight)
        }
    }

    func onDeleteSurface(_ surfaceId: String) {
        scrollView.subviews.forEach { $0.removeFromSuperview() }
    }
}
