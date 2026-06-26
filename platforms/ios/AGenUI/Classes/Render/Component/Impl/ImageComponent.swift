//
//  ImageComponent.swift
//  AGenUI
//
// Created on 2026/2/27.
//

import UIKit

/// ImageComponent component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - url: Image URL string (String)
/// - fit: Scale mode (String: 100%, contain, cover, adapt)
/// - variant: Semantic name (String: icon, avatar, smallFeature, mediumFeature, largeFeature, header)
/// - width: Image width for loading options (String)
/// - height: Image height for loading options (String)
///
/// Design notes:
/// - Uses ObservedImageView (custom UIImageView) with frame change monitoring for transition animations
/// - Integrates with ImageLoaderConfiguration for network image loading with caching
/// - Supports configurable transition animations (default: MagicRevealTransition, 1.5s duration)
/// - Animations only execute when surface.animationEnabled is true
class ImageComponent: Component {
    
    static var defaultTransition: ImageLoadTransition = MagicRevealTransition()
    /// Global default animation duration (seconds)
    static var defaultTransitionDuration: TimeInterval = 1.5
    
    // MARK: - Properties
    
    private var imageView: ObservedImageView?

    private var currentYogaWidth: Float = 0
    private var currentYogaHeight: Float = 0

    // Padding constraints — pin the inner imageView to self with adjustable insets so
    // CSS padding shrinks the image content from borderBox to contentBox.
    private var imageTopConstraint: NSLayoutConstraint?
    private var imageLeadingConstraint: NSLayoutConstraint?
    private var imageTrailingConstraint: NSLayoutConstraint?
    private var imageBottomConstraint: NSLayoutConstraint?
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "Image", properties: properties)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }


    override func createView(){
        let imageView = ObservedImageView()
        imageView.backgroundColor = .clear
        imageView.contentMode = .scaleAspectFit
        imageView.clipsToBounds = true
        imageView.image = nil
        imageView.translatesAutoresizingMaskIntoConstraints = false

        // Add image view
        addSubview(imageView)
        self.imageView = imageView

        // Pin imageView to self with constants that will be adjusted by CSS padding.
        let topC = imageView.topAnchor.constraint(equalTo: topAnchor, constant: 0)
        let leadingC = imageView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 0)
        let trailingC = imageView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: 0)
        let bottomC = imageView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: 0)
        NSLayoutConstraint.activate([topC, leadingC, trailingC, bottomC])
        self.imageTopConstraint = topC
        self.imageLeadingConstraint = leadingC
        self.imageTrailingConstraint = trailingC
        self.imageBottomConstraint = bottomC
        
        super.createView()
        
    }
    
    // MARK: - Measurement Override
    
    override class func measure(type: String, paramJson: String, maxWidth: Float, widthMode: MeasureMode, maxHeight: Float, heightMode: MeasureMode) -> CGSize {
        var measuredWidth: CGFloat = 0
        var measuredHeight: CGFloat = 0

        if (widthMode == .exactly || widthMode == .atMost) && maxWidth > 0 {
            measuredWidth = CGFloat(maxWidth)
        }

        if (heightMode == .exactly || heightMode == .atMost) && maxHeight > 0 {
            measuredHeight = CGFloat(maxHeight)
        }

        return CGSize(width: measuredWidth, height: measuredHeight)
    }
    
    // MARK: - Async Size Reporting

    private func reportImageRenderSize(image: UIImage?, yogaWidth: Float, yogaHeight: Float) {
        if yogaWidth != currentYogaWidth || yogaHeight != currentYogaHeight {
            return
        }

        if yogaWidth > 0 && yogaHeight > 0 {
            let wPt = CGFloat(yogaWidth) * 0.5
            let hPt = CGFloat(yogaHeight) * 0.5
            notifyLayoutChanged(width: wPt, height: hPt)
            return
        }

        guard let image = image else {
            notifyLayoutChanged(width: CGFloat(yogaWidth) * 0.5, height: CGFloat(yogaHeight) * 0.5)
            return
        }

        let imageSize = image.size
        guard imageSize.width > 0 && imageSize.height > 0 else {
            notifyLayoutChanged(width: CGFloat(yogaWidth) * 0.5, height: CGFloat(yogaHeight) * 0.5)
            return
        }

        let aspectRatio = imageSize.width / imageSize.height

        if yogaWidth > 0 {
            let wPt = CGFloat(yogaWidth) * 0.5
            let computedH = wPt / aspectRatio
            notifyLayoutChanged(width: wPt, height: computedH)
            return
        }

        if yogaHeight > 0 {
            let hPt = CGFloat(yogaHeight) * 0.5
            let computedW = hPt * aspectRatio
            notifyLayoutChanged(width: computedW, height: hPt)
            return
        }

        notifyLayoutChanged(width: imageSize.width, height: imageSize.height)
    }
    
    // MARK: - Component Override
    
    override func updateProperties(_ properties: [String: Any]) {
        // Call parent method to apply CSS properties to self
        super.updateProperties(properties)
        
        // Update scale mode (A2UI v0.9 protocol: fit)
        if let fit = properties["fit"] as? String {
            imageView?.contentMode = parseFit(fit)
        }

        var sizeFromReport = false
        var hasSizeChange = false
        var yogaWidth: Float = 0
        var yogaHeight: Float = 0

        // Apply CSS padding to the inner imageView.
        if let styles = properties["styles"] as? [String: Any] {
            applyImagePadding(styles)

            sizeFromReport = (styles["sizeFromReport"] as? Bool) == true
            hasSizeChange = styles["width"] != nil || styles["height"] != nil

            if !sizeFromReport {
                if let w = styles["width"] as? NSNumber {
                    yogaWidth = w.floatValue
                }
                if let h = styles["height"] as? NSNumber {
                    yogaHeight = h.floatValue
                }
                currentYogaWidth = yogaWidth
                currentYogaHeight = yogaHeight
            }
        }

        // Load image if url changed, or if size changed from CSS layout (not from our own report)
        if properties["url"] != nil || (hasSizeChange && !sizeFromReport && self.properties["url"] != nil) {
            let urlValue = properties["url"] ?? self.properties["url"]
            let url = urlValue as? String ?? ""
            loadImage(url, yogaWidth: yogaWidth, yogaHeight: yogaHeight)
        }
    }

    private func applyImagePadding(_ styles: [String: Any]) {
        guard let topC = imageTopConstraint,
              let leadingC = imageLeadingConstraint,
              let trailingC = imageTrailingConstraint,
              let bottomC = imageBottomConstraint else { return }

        guard CSSPaddingResolver.hasAnyPaddingKey(styles) else {
            // Reset to zero so prior padding does not stick when removed.
            topC.constant = 0
            leadingC.constant = 0
            trailingC.constant = 0
            bottomC.constant = 0
            return
        }

        let p = CSSPaddingResolver.resolve(styles)
        topC.constant = p.top
        leadingC.constant = p.left
        trailingC.constant = -p.right
        bottomC.constant = -p.bottom
    }
    
    override func setBorderRadius(_ radius: CGFloat) {
        super.setBorderRadius(radius)
        // Mirror corner radius to imageView so the image content is clipped to rounded corners.
        // imageView.clipsToBounds is already true (set in init).
        imageView?.layer.cornerRadius = radius
    }
    
    /// Parse scale mode
    /// A2UI v0.9 protocol values: fill, contain, cover, none, scaleDown, 100%, adapt
    ///
    /// CSS object-fit standard:
    ///   none       → no scaling, original size, overflow clipped
    ///   scaleDown → shrink to fit if larger than container, keep original if smaller
    ///
    /// Mapped to match Android ImageView.ScaleType:
    ///   contain    → FIT_CENTER    → .scaleAspectFit
    ///   cover      → CENTER_CROP   → .scaleAspectFill
    ///   fill       → FIT_XY        → .scaleToFill
    ///   none       → CENTER        → .center
    ///   scaleDown → CENTER_INSIDE → .scaleAspectFit (shrink if larger, also enlarge
    ///                if smaller — iOS has no exact CENTER_INSIDE; .scaleAspectFit is
    ///                closest: always shows complete image)
    private func parseFit(_ fit: String) -> UIView.ContentMode {
        switch fit.lowercased() {
        case "fill", "100%":
            return .scaleToFill
        case "contain":
            return .scaleAspectFit
        case "cover":
            return .scaleAspectFill
        case "none":
            // none maps to fill: stretch to fill container, may distort
            return .scaleToFill
        case "scaleDown":
            // CSS object-fit: scaleDown — shrink to fit if larger, keep original if smaller
            return .scaleAspectFit
        case "adapt":
            return .scaleAspectFit
        default:
            return .scaleAspectFit
        }
    }
    
    // MARK: - Private Methods
    
    /// Load image
    private func loadImage(_ src: String, yogaWidth: Float, yogaHeight: Float) {
        guard imageView != nil else { return }
        
        // Cancel previous loading tasks
        cancelCurrentLoadTask()
        
        if src.isEmpty {
            return
        }

        loadNetworkImage(from: src, yogaWidth: yogaWidth, yogaHeight: yogaHeight)
    }
    
    /// Current loading task identifier (for cancellation)
    private var currentTaskId: String?
    
    /// Cancel current image loading task
    private func cancelCurrentLoadTask() {
        guard let taskId = currentTaskId else { return }
        ImageLoaderConfiguration.shared.loader.cancel(for: taskId)
        currentTaskId = nil
    }
    
    /// Load network image (using ImageLoaderConfiguration)
    private func loadNetworkImage(from urlString: String, yogaWidth: Float, yogaHeight: Float) {
        guard let imageView = imageView, let url = URL(string: urlString) else {
            Logger.shared.debug("⚠️ [ImageComponent] Invalid URL or imageView is nil: \(urlString)")
            return
        }
        
        // Clear current taskId (cancellation handled at start of loadImage)
        currentTaskId = nil
        
        // Use globally configured ImageLoader to load image
        // Pass width/height from properties if present, otherwise not
        var options: [String: Any] = [
            ImageLoadOptionsKey.surfaceId: surface?.surfaceId ?? "",
            ImageLoadOptionsKey.componentId: componentId
        ]
        let w = CGFloat(yogaWidth) * 0.5
        let h = CGFloat(yogaHeight) * 0.5
        if w > 0 {
            options[ImageLoadOptionsKey.width] = w
        }
        if h > 0 {
            options[ImageLoadOptionsKey.height] = h
        }
        
        let taskId = ImageLoaderConfiguration.shared.loader.loadImage(from: url, options: options) { [weak self] image, isFromCache, error in
            guard let self else { return }
            
            DispatchQueue.main.async {
                if let image = image {
                    guard let imageView = self.imageView else { return }
                    
                    imageView.image = image
                    self.reportImageRenderSize(image: image, yogaWidth: yogaWidth, yogaHeight: yogaHeight)
                    
                    // Execute transition animation (only when surface animation is enabled)
                    if self.surface?.animationEnabled == true {
                        imageView.executeTransitionAnimation()
                    }
                } else if let error = error {
                    // Log non-cancel errors for debugging
                    if error as? ImageLoaderError != .cancelled {
                        Logger.shared.warning("[ImageComponent] Failed to load image: \(urlString), componentId: \(self.componentId), error: \(error.localizedDescription)")
                    }
                    self.reportImageRenderSize(image: nil, yogaWidth: yogaWidth, yogaHeight: yogaHeight)
                } else {
                    // No image and no error - unexpected state
                    Logger.shared.warning("[ImageComponent] Image load returned nil without error: \(urlString), componentId: \(self.componentId)")
                    self.reportImageRenderSize(image: nil, yogaWidth: yogaWidth, yogaHeight: yogaHeight)
                }
            }
        }
        
        // Save current taskId
        currentTaskId = taskId
    }
    
    /// ImageView supporting height change monitoring
    class ObservedImageView: UIImageView {
        private var pendingAnimation: Bool = false
        private var isTransitioning: Bool = false
        
        override var frame: CGRect {
            didSet {
                handleFrameChange(frame)
            }
        }
        
        /// Request animation execution (called when image loading completes)
        public func executeTransitionAnimation() {
            if frame.height > 1 && frame.width > 1 {
                performAnimation()
            } else {
                pendingAnimation = true
            }
        }
        
        private func handleFrameChange(_ frame: CGRect) {
            guard frame.height > 1, frame.width > 1, pendingAnimation else { return }
            
            pendingAnimation = false
            performAnimation()
        }
        
        private func performAnimation() {
            guard !isTransitioning else { return }
            
            isTransitioning = true
            ImageComponent.defaultTransition.animate(
                on: self,
                duration: ImageComponent.defaultTransitionDuration
            ) { [weak self] in
                self?.isTransitioning = false
            }
        }
    }
}
