//
//  AppDelegate.swift
//  Playground
//
//  Created by yinglong.zyl on 04/20/2026.
//  Copyright (c) 2026 yinglong.zyl. All rights reserved.
//

import UIKit
import AGenUI

@main
class AppDelegate: UIResponder, UIApplicationDelegate {

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Configure global theme color
        setupAppearance()
        // Inject custom runtime logger into AGenUI module
        setupRuntimeLogger()
        // Register custom fonts from bundle
        registerCustomFonts()
        return true
    }

    // MARK: - Logger Setup

    private func setupRuntimeLogger() {
        AGenUISDK.setCustomLogger(PlaygroundRuntimeLogger.shared)
    }
    
    // MARK: - Appearance Configuration
    
    /// Configure global theme color
    private func setupAppearance() {
        // Define magenta color
        let magentaColor = UIColor(red: 255/255.0, green: 45/255.0, blue: 85/255.0, alpha: 1.0)
        
        // Set global tintColor
        UIView.appearance().tintColor = magentaColor
        
        // Set navigation bar button color
        UINavigationBar.appearance().tintColor = magentaColor
        
        // Set TabBar selected color
        UITabBar.appearance().tintColor = magentaColor
        
        // Set button color
        UIButton.appearance().tintColor = magentaColor
        
        // Set switch control color
        UISwitch.appearance().onTintColor = magentaColor
        
        // Set progress bar color
        UIProgressView.appearance().tintColor = magentaColor
        
        // Set slider color
        UISlider.appearance().tintColor = magentaColor
    }

    // MARK: - Custom Font Registration

    private func registerCustomFonts() {
        // ---- Method 1: registerFont(_:fileURL:) ----
        // Load .ttf files from the app bundle at runtime.
        let bundleFonts: [(cssName: String, resource: String)] = [
            ("Nunito", "Nunito-Regular"),
            ("PlayfairDisplay", "PlayfairDisplay-Regular"),
            ("FiraCode", "FiraCode-Regular"),
        ]
        // Scan bundle for all ttf files and register by matching name
        let bundlePath = Bundle.main.bundlePath
        let allFiles = (try? FileManager.default.contentsOfDirectory(atPath: bundlePath)) ?? []
        let ttfFiles = allFiles.filter { $0.hasSuffix(".ttf") }

        for entry in bundleFonts {
            let fileName = entry.resource + ".ttf"
            if ttfFiles.contains(fileName) {
                let fileURL = URL(fileURLWithPath: bundlePath).appendingPathComponent(fileName)
                let ok = AGenUISDK.registerFont(entry.cssName, fileURL: fileURL)
                if !ok {
                    NSLog("[AGenUI] registerFont(%@) failed", entry.cssName)
                }
            }
        }

        // Method 2 (registerFont(_:fontName:)) can be used for system-installed
        // fonts that need a CSS alias. Not currently needed.
    }

    // MARK: UISceneSession Lifecycle

    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        // Called when a new scene session is being created.
        // Use this method to select a configuration to create the new scene with.
        return UISceneConfiguration(name: "Default Configuration", sessionRole: connectingSceneSession.role)
    }

    func application(_ application: UIApplication, didDiscardSceneSessions sceneSessions: Set<UISceneSession>) {
        // Called when the user discards a scene session.
        // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
        // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
    }


}
