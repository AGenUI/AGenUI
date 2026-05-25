//
//  DefaultImageLoader.swift
//  AGenUI
//
// Created on 2026/4/9.
//

import UIKit
import ImageIO

/// Default image loader
///
/// Features:
/// - Uses URLSession for network requests
/// - Two-level caching: NSCache memory cache + disk cache
/// - Supports cancellation by URL
public class DefaultImageLoader: ImageLoader {
    
    // MARK: - Properties
    
    private let session: URLSession
    private let memoryCache = NSCache<NSString, UIImage>()
    private let diskCache: DiskImageCache
    
    /// Maximum pixel dimension for decoding. Images larger than this will be downsampled.
    /// 2048px covers Retina displays (1024pt @2x) while keeping decoded bitmap under 16MB.
    private static let maxDecodedPixelSize: CGFloat = 2048
    
    /// Task info (stores dataTask and completion)
    private struct TaskInfo {
        let dataTask: URLSessionDataTask
        let completion: (UIImage?, Bool, Error?) -> Void
    }
    private var tasks: [String: TaskInfo] = [:]
    private let lock = NSLock()
    
    // MARK: - Initialization
    
    public init(session: URLSession = .shared, diskCacheConfiguration: DiskImageCache.Configuration? = nil) {
        self.session = session
        // Use totalCostLimit (bytes) instead of countLimit for better memory control
        // 50MB limit for memory cache
        self.memoryCache.totalCostLimit = 50 * 1024 * 1024
        self.diskCache = DiskImageCache(configuration: diskCacheConfiguration ?? DiskImageCache.Configuration())
        
        // Clean expired disk cache on startup (async to avoid blocking init)
        DispatchQueue.global(qos: .background).async { [weak self] in
            self?.diskCache.cleanExpired()
        }
    }
    
    // MARK: - ImageLoader
    
    @discardableResult
    public func loadImage(
        from url: URL,
        options: [String: Any]? = nil,
        completion: @escaping (UIImage?, Bool, Error?) -> Void
    ) -> String {
        let taskId = url.absoluteString
        
        // 1. Check memory cache
        let cacheKey = url.absoluteString as NSString
        if let cachedImage = memoryCache.object(forKey: cacheKey) {
            DispatchQueue.main.async {
                completion(cachedImage, true, nil)
            }
            return taskId
        }
        
        // 2. Check disk cache and decode on background thread
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self = self else { return }
            
            // Read raw data from disk cache for memory-efficient ImageIO downsampling
            guard let diskData = self.diskCache.data(for: url) else {
                // Disk cache miss, initiate network request
                self.startNetworkTask(for: url, taskId: taskId, cacheKey: cacheKey, completion: completion)
                return
            }
            
            // Downsample directly from data (avoids full-resolution bitmap allocation)
            guard let decodedImage = self.downsampledImage(from: diskData) else {
                self.startNetworkTask(for: url, taskId: taskId, cacheKey: cacheKey, completion: completion)
                return
            }
            
            // Write to memory cache with cost for proper eviction
            let cost = self.imageCost(decodedImage)
            self.memoryCache.setObject(decodedImage, forKey: cacheKey, cost: cost)
            
            DispatchQueue.main.async {
                completion(decodedImage, true, nil)
            }
        }
        
        return taskId
    }
    
    public func cancel(for taskId: String) {
        lock.lock()
        if let taskInfo = tasks.removeValue(forKey: taskId) {
            taskInfo.dataTask.cancel()
            // Callback cancellation status
            DispatchQueue.main.async {
                taskInfo.completion(nil, false, ImageLoaderError.cancelled)
            }
        }
        lock.unlock()
    }
    
    // MARK: - Private Methods
    
    /// Initiate network request
    private func startNetworkTask(
        for url: URL,
        taskId: String,
        cacheKey: NSString,
        completion: @escaping (UIImage?, Bool, Error?) -> Void
    ) {
        let task = session.dataTask(with: url) { [weak self] data, response, error in
            guard let self = self else { return }
            
            // Remove from dictionary after task completes
            self.lock.lock()
            self.tasks.removeValue(forKey: taskId)
            self.lock.unlock()
            
            if let error = error {
                // When URLSession cancels a task, error is NSURLErrorCancelled
                let nsError = error as NSError
                if nsError.code == NSURLErrorCancelled {
                    DispatchQueue.main.async {
                        completion(nil, false, ImageLoaderError.cancelled)
                    }
                } else {
                    DispatchQueue.main.async {
                        completion(nil, false, ImageLoaderError.networkError(error))
                    }
                }
                return
            }
            
            guard let data = data else {
                DispatchQueue.main.async {
                    completion(nil, false, ImageLoaderError.invalidData)
                }
                return
            }
            
            // Decode image on background thread
            DispatchQueue.global(qos: .userInitiated).async {
                // Use ImageIO to downsample directly from data (memory-efficient for large images)
                guard let image = self.downsampledImage(from: data) else {
                    DispatchQueue.main.async {
                        completion(nil, false, ImageLoaderError.invalidData)
                    }
                    return
                }
                
                // Two-level cache (store original data to disk, decoded+downsampled to memory)
                let cost = self.imageCost(image)
                self.memoryCache.setObject(image, forKey: cacheKey, cost: cost)
                self.diskCache.store(data, for: url)
                
                // Callback
                DispatchQueue.main.async {
                    completion(image, false, nil)
                }
            }
        }
        
        lock.lock()
        tasks[taskId] = TaskInfo(dataTask: task, completion: completion)
        lock.unlock()
        
        task.resume()
    }
    
    // MARK: - Image Decoding & Downsampling
    
    /// Downsample image from raw data using ImageIO.
    /// This is the most memory-efficient path: ImageIO decodes at the target size
    /// without ever allocating a full-resolution bitmap.
    private func downsampledImage(from data: Data) -> UIImage? {
        let options: [CFString: Any] = [kCGImageSourceShouldCache: false]
        guard let source = CGImageSourceCreateWithData(data as CFData, options as CFDictionary) else {
            return nil
        }
        return createThumbnail(from: source)
    }
    
    /// Create a downsampled and decoded thumbnail from a CGImageSource.
    private func createThumbnail(from source: CGImageSource) -> UIImage? {
        let thumbnailOptions: [CFString: Any] = [
            kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceThumbnailMaxPixelSize: Self.maxDecodedPixelSize,
            kCGImageSourceShouldCacheImmediately: true  // Force decode at thumbnail size
        ]
        guard let cgImage = CGImageSourceCreateThumbnailAtIndex(source, 0, thumbnailOptions as CFDictionary) else {
            return nil
        }
        return UIImage(cgImage: cgImage)
    }
    
    /// Calculate approximate decoded memory cost for NSCache eviction.
    private func imageCost(_ image: UIImage) -> Int {
        guard let cgImage = image.cgImage else { return 0 }
        return cgImage.bytesPerRow * cgImage.height
    }
    
    // MARK: - Cache Management
    
    public func clearMemory() {
        memoryCache.removeAllObjects()
    }
    
    public func clearDisk() {
        diskCache.removeAll()
    }
}
