//
//  ImageLoaderConfiguration.swift
//  AGenUI
//
// Created on 2026/4/9.
//

import UIKit

/// Image loader global configuration
class ImageLoaderConfiguration {
    
    /// Singleton
    static let shared = ImageLoaderConfiguration()
    
    /// Current image loader
    /// - Replace this property to take effect globally
    /// - Default uses DefaultImageLoader (based on URLSession + memory cache)
    var loader: ImageLoader
    
    /// Private initialization
    private init() {
        self.loader = DefaultImageLoader()
    }
}
