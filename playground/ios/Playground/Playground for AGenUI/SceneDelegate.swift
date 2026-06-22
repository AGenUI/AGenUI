//
//  SceneDelegate.swift
//  Playground
//
//  Created on 2026/3/20.
//

import UIKit

class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?


    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = (scene as? UIWindowScene) else { return }
        
        let window = UIWindow(windowScene: windowScene)
        
        // Check launch arguments for test mode
        let args = ProcessInfo.processInfo.arguments
        var testDataPath: String? = nil
        var testBundleDataB64: String? = nil
        
        // Check for stability test mode (must be before other test checks)
        if args.contains("--stability-test") {
            let config = StabilityTestConfig.fromLaunchArgs(args)
            window.rootViewController = StabilityTestViewController(config: config)
            window.makeKeyAndVisible()
            self.window = window
            return
        }
        
        // Support --testCase <id> (resolves to standard bundle path)
        if let idx = args.firstIndex(of: "--testCase"), idx + 1 < args.count {
            let caseId = args[idx + 1]
            testDataPath = "tmp/agenui_test/\(caseId)/\(caseId)_bundle.json"
        }
        // Support --testDataPath <path> (direct file path)
        if let idx = args.firstIndex(of: "--testDataPath"), idx + 1 < args.count {
            testDataPath = args[idx + 1]
        }
        // Support --testBundleDataB64 <base64> (inline data, no file needed)
        if let idx = args.firstIndex(of: "--testBundleDataB64"), idx + 1 < args.count {
            testBundleDataB64 = args[idx + 1]
        }
        
        // Also check URL scheme: agenui://test?b64=<base64data>
        if let urlContext = connectionOptions.urlContexts.first {
            let url = urlContext.url
            if url.host == "test",
               let components = URLComponents(url: url, resolvingAgainstBaseURL: false),
               let b64 = components.queryItems?.first(where: { $0.name == "b64" })?.value {
                testBundleDataB64 = b64
            }
        }
        
        let rootVC: UIViewController
        if let b64 = testBundleDataB64,
           let data = Data(base64Encoded: b64),
           let jsonStr = String(data: data, encoding: .utf8) {
            // Test mode with inline base64 data
            rootVC = UITestViewController(bundleJsonString: jsonStr)
        } else if let path = testDataPath {
            // Test mode with file path
            rootVC = UITestViewController(testDataPath: path)
        } else {
            // Normal mode: Playground
            let playgroundVC = A2UIPlaygroundViewController()
            rootVC = UINavigationController(rootViewController: playgroundVC)
            
            // Support --clear-logs (explicit, independent of --gallery).
            // Only clear sandbox logs when explicitly requested; otherwise per-round
            // cold starts (which always carry --gallery) would wipe previous rounds' logs,
            // leaving only the last round's log file in the sandbox.
            if args.contains("--clear-logs") {
                clearSandboxLogs()
            }

            // Support --gallery (auto-navigate to Gallery template on launch)
            let autoGallery = args.contains("--gallery")
            if autoGallery {
                // Auto-load Gallery template after view setup
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                    playgroundVC.autoLoadGalleryTemplate()
                }
            }
        }
        
        window.rootViewController = rootVC
        window.makeKeyAndVisible()
        self.window = window
    }

    func sceneDidDisconnect(_ scene: UIScene) {
        // Called as the scene is being released by the system.
        // This occurs shortly after the scene enters the background, or when its session is discarded.
        // Release any resources associated with this scene that can be re-created the next time the scene connects.
        // The scene may re-connect later, as its session was not necessarily discarded (see `application:didDiscardSceneSessions` instead).
    }

    func sceneDidBecomeActive(_ scene: UIScene) {
        // Called when the scene has moved from an inactive state to an active state.
        // Use this method to restart any tasks that were paused (or not yet started) when the scene was inactive.
    }

    func sceneWillResignActive(_ scene: UIScene) {
        // Called when the scene will move from an active state to an inactive state.
        // This may occur due to temporary interruptions (ex. an incoming phone call).
    }

    func sceneWillEnterForeground(_ scene: UIScene) {
        // Called as the scene transitions from the background to the foreground.
        // Use this method to undo the changes made on entering the background.
    }

    func sceneDidEnterBackground(_ scene: UIScene) {
        // Called as the scene transitions from the foreground to the background.
        // Use this method to save data, release shared resources, and store enough scene-specific state information
        // to restore the scene back to its current state.
    }

    func scene(_ scene: UIScene, openURLContexts URLContexts: Set<UIOpenURLContext>) {
        guard let url = URLContexts.first?.url else { return }
        handleDeepLink(url: url)
    }

    private func handleDeepLink(url: URL) {
        if url.scheme == "agenui", url.host == "clear-logs" {
            self.clearSandboxLogs()
            print("✅ [DeepLink] Received clearSandboxLogs")
            return
        }
        
        guard url.scheme == "playground",
              url.host == "a2ui_test" else {
            return
        }

        // Parse URL query parameters
        guard let components = URLComponents(url: url, resolvingAgainstBaseURL: false),
              let queryItems = components.queryItems,
              let fileUrl = queryItems.first(where: { $0.name == "url" })?.value else {
            print("[DeepLink] Missing 'url' parameter in: \(url)")
            return
        }

        print("[DeepLink] Received a2ui_test URL: \(fileUrl)")

        // Get A2UIPlaygroundViewController and call download handler
        DispatchQueue.main.async {
            guard let navController = self.window?.rootViewController as? UINavigationController,
                  let playgroundVC = navController.viewControllers.first as? A2UIPlaygroundViewController else {
                print("[DeepLink] Failed to get A2UIPlaygroundViewController")
                return
            }
            playgroundVC.downloadAndProcessQRCodeFile(fileUrl)
        }
    }

    // MARK: - Sandbox Log Management
    
    /// Clear the agenui_log directory in the sandbox
    private func clearSandboxLogs() {
        let fileManager = FileManager.default
        
        // Get the Documents directory
        guard let documentsPath = fileManager.urls(for: .documentDirectory, in: .userDomainMask).first else {
            print("⚠️ [SandboxLog] Failed to get Documents directory")
            return
        }
        
        let agenuiLogPath = documentsPath.appendingPathComponent("agenui_log")
        
        // Check whether the directory exists
        var isDir: ObjCBool = false
        guard fileManager.fileExists(atPath: agenuiLogPath.path, isDirectory: &isDir), isDir.boolValue else {
            print("ℹ️ [SandboxLog] agenui_log directory does not exist, no need to clear")
            return
        }
        
        do {
            // Get all files and subdirectories under the directory
            let contents = try fileManager.contentsOfDirectory(atPath: agenuiLogPath.path)
            
            var deletedCount = 0
            for item in contents {
                let itemPath = agenuiLogPath.appendingPathComponent(item)
                try fileManager.removeItem(at: itemPath)
                deletedCount += 1
            }
            
            print("✅ [SandboxLog] Successfully cleared agenui_log directory: \(deletedCount) items deleted")
            print("📁 [SandboxLog] Path: \(agenuiLogPath.path)")
        } catch {
            print("❌ [SandboxLog] Failed to clear agenui_log directory: \(error.localizedDescription)")
        }
    }

}

