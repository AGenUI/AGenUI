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

    // Padding constraints — pin the inner imageView to self with adjustable insets so
    // CSS padding shrinks the image content from borderBox to contentBox.
    private var imageTopConstraint: NSLayoutConstraint?
    private var imageLeadingConstraint: NSLayoutConstraint?
    private var imageTrailingConstraint: NSLayoutConstraint?
    private var imageBottomConstraint: NSLayoutConstraint?
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "Image", properties: properties)
        
        // Create inner image view - only handles image rendering and contentMode
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
        
        // Apply initial properties
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Measurement Override
    
    override class func measure(type: String, paramJson: String, maxWidth: Float, widthMode: MeasureMode, maxHeight: Float, heightMode: MeasureMode) -> CGSize {
        let defaultSize: CGFloat = 0
        var measuredWidth = defaultSize
        var measuredHeight = defaultSize

        if (widthMode == .exactly || widthMode == .atMost) && maxWidth > 0 {
            measuredWidth = widthMode == .atMost
                ? min(measuredWidth, CGFloat(maxWidth))
                : CGFloat(maxWidth)
        }
        if (heightMode == .exactly || heightMode == .atMost) && maxHeight > 0 {
            measuredHeight = CGFloat(maxHeight)
        }

        return CGSize(width: measuredWidth, height: measuredHeight)
    }
    
    // MARK: - Component Override
    
    override func updateProperties(_ properties: [String: Any]) {
        // Call parent method to apply CSS properties to self
        super.updateProperties(properties)
        
        // Update image URL (A2UI v0.9 protocol: url)
        if let urlValue = properties["url"] {
            let url = urlValue as? String ?? ""
            loadImage(url)
        }
        
        // Update scale mode (A2UI v0.9 protocol: fit)
        if let fit = properties["fit"] as? String {
            imageView?.contentMode = parseFit(fit)
        }

        // Apply CSS padding to the inner imageView.
        //
        // W3C: <img> is a replaced element and supports padding — the bitmap
        // is laid out within the content-box, with padding rendered as the
        // element's background extending around it. The C++ Yoga engine sets
        // padding on the leaf node, so the ImageComponent's own frame is the
        // borderBox and `measure` receives the contentBox. The inner imageView
        // is pinned to self via anchor constraints; translating CSS padding
        // into the four edge constants shrinks it down to the contentBox.
        if let styles = properties["styles"] as? [String: Any] {
            applyImagePadding(styles)
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
    private func loadImage(_ src: String) {
        guard imageView != nil else { return }
        
        // Cancel previous loading tasks
        cancelCurrentLoadTask()
        
        if src.isEmpty {
            return
        }
        
        loadNetworkImage(from: src)
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
    private func loadNetworkImage(from urlString: String) {
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
        let w = frame.size.width
        let h = frame.size.height
        if w > 0 && h > 0 {
            options[ImageLoadOptionsKey.width] = w
            options[ImageLoadOptionsKey.height] = h
        }
        
        let taskId = ImageLoaderConfiguration.shared.loader.loadImage(from: url, options: options) { [weak self] image, isFromCache, error in
            guard let self else { return }
            
            DispatchQueue.main.async {
                if let image = image {
                    guard let imageView = self.imageView else { return }
                    
                    imageView.image = image
                    
                    var newSize = self.frame.size
                    let imageSize = image.size
                    var sizeChange = false
                    if imageSize.width > 0 && imageSize.height > 0 {
                        if newSize.width <= 0 {
                            newSize.width = imageSize.width
                            sizeChange = true
                        }
                        
                        if newSize.height <= 0 {
                            newSize.height = imageSize.height
                            sizeChange = true
                        }
                    }
                    // If current component size is (0, 0), notify Yoga with the image's intrinsic size
                    if sizeChange == true {
                        self.notifyLayoutChanged(width: newSize.width, height: newSize.height)
                    }
                    
                    // Execute transition animation (only when surface animation is enabled)
                    if self.surface?.animationEnabled == true {
                        imageView.executeTransitionAnimation()
                    }
                } else if let error = error {
                    // Log non-cancel errors for debugging
                    if error as? ImageLoaderError != .cancelled {
                        Logger.shared.warning("[ImageComponent] Failed to load image: \(urlString), componentId: \(self.componentId), error: \(error.localizedDescription)")
                    }
                } else {
                    // No image and no error - unexpected state
                    Logger.shared.warning("[ImageComponent] Image load returned nil without error: \(urlString), componentId: \(self.componentId)")
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
