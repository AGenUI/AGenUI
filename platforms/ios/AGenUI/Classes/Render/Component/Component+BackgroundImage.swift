//
//  Component+BackgroundImage.swift
//  AGenUI
//
// Created on 2026/11/23.
//

import UIKit

extension Component {

    // MARK: - Entry

    /// Applies the `background-image` declaration.
    ///
    /// Mirrors `ImageComponent.loadImage`: cancel the previous task, then load
    /// through ImageLoader. Stale-callback protection relies on the loader's
    /// cancel contract exactly as ImageComponent does — no generation token and
    /// no url bookkeeping. The only behavioural difference is that an empty /
    /// malformed declaration clears the image layer (background-image is a style
    /// declaration, unlike Image's `url` content which keeps the last bitmap).
    ///
    /// This layer only strips the url() wrapper; url validation and protocol
    /// routing belong to ImageLoader (the host `A2UIImageLoader` decides what it
    /// can load), so no prefix is re-checked here.
    @MainActor func applyBackgroundImage(rawStyle: Any?) {
        // Cancel previous loading task
        cancelBackgroundImageLoad()

        let url = Self.parseBackgroundImageURL(from: rawStyle)
        guard !url.isEmpty, let u = URL(string: url) else {
            clearBackgroundImageLayer()
            return
        }

        let loader = ImageLoaderConfiguration.shared.loader
        backgroundImageTaskId = loader.loadImage(from: u, options: backgroundImageLoadOptions()) { [weak self] image, _, error in
            guard let self else { return }

            DispatchQueue.main.async {
                if let image {
                    self.renderBackgroundImage(image)
                } else if let error, !Self.isBackgroundImageCancellation(error) {
                    // Log non-cancel errors for debugging; keep the previous image,
                    // matching ImageComponent's failure handling.
                    Logger.shared.warning("[bg-image] load failed, component=\(self.componentId) url=\(url) error=\(error.localizedDescription)")
                }
            }
        }
    }

    /// Cancel current background-image loading task. Called before a new load and
    /// when the body leaves the view tree (`removeFromSuperview`).
    @MainActor func cancelBackgroundImageLoad() {
        guard let taskId = backgroundImageTaskId else { return }
        ImageLoaderConfiguration.shared.loader.cancel(for: taskId)
        backgroundImageTaskId = nil
    }

    // MARK: - Private

    @MainActor private func clearBackgroundImageLayer() {
        guard layer.contents != nil else { return }   // Idempotent: a styles re-apply must not touch the layer
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        layer.contents = nil
        CATransaction.commit()
    }

    @MainActor private func renderBackgroundImage(_ image: UIImage) {
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        layer.contents = image.cgImage
        layer.contentsGravity = .resizeAspectFill   // COVER — the tri-platform baseline, do not change
        CATransaction.commit()
    }

    /// Pixel-sized load options. `applyLayoutFromStyles` runs before the CSS pass,
    /// so bounds are already resolved here. The host `A2UIImageLoader` only triggers
    /// CDN cropping (`cutImageURL`) when BOTH width and height are > 0, and reads
    /// them as CGFloat. Same convention as `ImageComponent.loadNetworkImage`.
    @MainActor private func backgroundImageLoadOptions() -> [String: Any] {
        var options: [String: Any] = [
            ImageLoadOptionsKey.componentId: componentId,
            ImageLoadOptionsKey.surfaceId: surface?.surfaceId ?? ""
        ]
        let scale = UIScreen.main.scale
        let w = bounds.width * scale
        let h = bounds.height * scale
        if w > 0, h > 0 {
            options[ImageLoadOptionsKey.width] = w
            options[ImageLoadOptionsKey.height] = h
        }
        return options
    }

    // MARK: - Parsing

    /// Absent key or malformed url() → "", which clears the image layer.
    /// Equivalent to the former `ifPresent(defaultValue: .url(""))` fallback.
    static func parseBackgroundImageURL(from raw: Any?) -> String {
        guard let raw else { return "" }
        let s = (raw as? String) ?? "\(raw)"
        guard case .url(let u) = CSSPropertyParser.parse(value: s, valueType: .url) else { return "" }
        return u.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    /// The host loader reports cancellation as a GDImageLoader NSError rather than
    /// `ImageLoaderError.cancelled`, so both shapes are recognised to avoid logging
    /// a deliberate cancel as a failure.
    static func isBackgroundImageCancellation(_ error: Error) -> Bool {
        if let e = error as? ImageLoaderError, e == .cancelled { return true }
        if let e = error as NSError?, e.code == NSURLErrorCancelled { return true }
        return false
    }
}
