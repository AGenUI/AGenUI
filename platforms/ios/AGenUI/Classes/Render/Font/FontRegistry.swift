import Foundation
import CoreText

/// Global registry for custom fonts loaded at runtime.
///
/// Provides two registration paths:
/// - ``registerFont(familyName:fileURL:)`` — loads a `.ttf`/`.otf` from disk
///   via `CTFontManager`, discovers its PostScript name, and stores the mapping.
/// - ``registerFont(familyName:fontName:)`` — maps a CSS family name to a font
///   name already available in the system (e.g. declared in `Info.plist`).
///
/// Registered names are looked up during `font-family` CSS resolution in
/// `TextComponent.buildFont()`.
///
/// Thread-safe: all access is serialised by an `NSLock`.
public final class FontRegistry {

    public static let shared = FontRegistry()

    /// familyName (lowercased) → resolved font name usable by UIFont
    private var registry: [String: String] = [:]
    private let lock = NSLock()

    private init() {}

    // MARK: - Registration

    /// Register a custom font from a file URL.
    ///
    /// The font is registered with the system via `CTFontManager` and becomes
    /// available for `font-family` CSS property resolution in all text components.
    ///
    /// - Parameters:
    ///   - familyName: the name authors use in `font-family` (case-insensitive)
    ///   - fileURL: URL pointing to a `.ttf` or `.otf` font file
    /// - Returns: `true` if registration succeeds
    @discardableResult
    public func registerFont(familyName: String, fileURL: URL) -> Bool {
        guard !familyName.isEmpty else { return false }

        var errorRef: Unmanaged<CFError>?
        let success = CTFontManagerRegisterFontsForURL(fileURL as CFURL, .process, &errorRef)
        if !success {
            return false
        }

        // Discover the PostScript name from the registered file
        if let descriptors = CTFontManagerCreateFontDescriptorsFromURL(fileURL as CFURL) as? [CTFontDescriptor],
           let firstDesc = descriptors.first,
           let postScriptName = CTFontDescriptorCopyAttribute(firstDesc, kCTFontNameAttribute) as? String {
            store(familyName: familyName, resolvedName: postScriptName)
        } else {
            store(familyName: familyName, resolvedName: familyName)
        }
        return true
    }

    /// Register a font name already available in the system.
    ///
    /// Use this when the font is declared in `Info.plist` under `UIAppFonts`
    /// and you want to expose it under a custom CSS family name.
    ///
    /// - Parameters:
    ///   - familyName: the name authors use in `font-family` (case-insensitive)
    ///   - fontName: the actual font name (PostScript name or family name)
    /// - Returns: `true` always (name-only mapping, no validation)
    @discardableResult
    public func registerFont(familyName: String, fontName: String) -> Bool {
        guard !familyName.isEmpty, !fontName.isEmpty else { return false }
        store(familyName: familyName, resolvedName: fontName)
        return true
    }

    // MARK: - Lookup

    /// Look up a registered font name by CSS family name.
    ///
    /// - Returns: the resolved font name, or `nil` if not registered
    public func resolve(familyName: String) -> String? {
        let key = familyName.lowercased()
        lock.lock()
        defer { lock.unlock() }
        return registry[key]
    }

    // MARK: - Private

    private func store(familyName: String, resolvedName: String) {
        let key = familyName.lowercased()
        lock.lock()
        defer { lock.unlock() }
        registry[key] = resolvedName
    }
}
