//
//  TextComponent.swift
//  AGenUI
//
// Created on 2026/2/27.
//

import UIKit
import CoreText

// MARK: - Custom Attribute Key

extension NSAttributedString.Key {
    /// Custom decoration attribute, value is TextDecorationConfigBox
    /// Used only for custom Core Graphics drawing of .dashed style
    static let customDecoration = NSAttributedString.Key("CustomTextDecorationAttributeName")
}

/// Box wrapper for storing TextDecorationConfig in NSAttributedString (requires ObjC object)
private class TextDecorationConfigBox: NSObject {
    let config: TextDecorationConfig
    init(_ config: TextDecorationConfig) {
        self.config = config
        super.init()
    }
}

// MARK: - Text Decoration Configuration (W3C Standard)

/// Text decoration line position (W3C text-decoration-line)
enum TextDecorationLine: String {
    case none = "none"
    case underline = "underline"
    case lineThrough = "line-through"
}

/// Text decoration line style (W3C text-decoration-style)
enum TextDecorationStyle: String {
    case solid = "solid"
    case double = "double"
    case dotted = "dotted"
    case dashed = "dashed"
    case wavy = "wavy"
}

/// Text decoration configuration (W3C compliant)
struct TextDecorationConfig {
    /// Decoration line position
    var line: TextDecorationLine = .none
    /// Decoration line style
    var style: TextDecorationStyle = .solid
    /// Decoration line color
    var color: UIColor? = nil
    /// Decoration line thickness (W3C text-decoration-thickness)
    var thickness: CGFloat = 1.0
    /// Dashed line segment width (only for .dashed style, default 2)
    var dashWidth: CGFloat = 2
    /// Dashed line gap width (only for .dashed style, default 1.5)
    var dashGap: CGFloat = 1.5
    /// Extra underline offset from baseline position (default 1)
    var offset: CGFloat = 1
    
    /// Parse from CSS style dictionary
    static func from(styles: [String: Any]) -> TextDecorationConfig? {
        var config = TextDecorationConfig()
        
        // Parse shorthand property text-decoration
        if let decoration = styles["text-decoration"] as? String {
            parseTextDecoration(decoration, into: &config)
        }
        
        // Parse text-decoration-line
        if let lineValue = styles["text-decoration-line"] as? String {
            if let line = TextDecorationLine(rawValue: lineValue.lowercased()) {
                config.line = line
            }
        }
        
        // Parse text-decoration-style
        if let styleValue = styles["text-decoration-style"] as? String {
            if let style = TextDecorationStyle(rawValue: styleValue.lowercased()) {
                config.style = style
            }
        }
        
        // Parse text-decoration-color
        if let colorValue = styles["text-decoration-color"] as? String {
            let parsedColor = CSSPropertyParser.parseColor(colorValue)
            if case .color(let value) = parsedColor {
                config.color = value
            }
        }
        
        // Parse text-decoration-thickness
        if let thicknessValue = styles["text-decoration-thickness"] as? String {
            if let thickness = parseLength(thicknessValue), thickness > 0 {
                config.thickness = thickness
            }
        } else if let thicknessValue = styles["text-decoration-thickness"] as? CGFloat, thicknessValue > 0 {
            config.thickness = thicknessValue * Component.BS_POINT_SCALE
        }
        
        return config.line != TextDecorationLine.none ? config : nil
    }
    
    /// Parse text-decoration shorthand property (positional format, aligned with Android)
    /// Format: "line style color" (e.g. "underline dashed #FF0000")
    /// - parts[0]: line type (underline, line-through, none)
    /// - parts[1]: style (solid, dashed, dotted, double, wavy)
    /// - parts[2]: color (#hex or rgb)
    private static func parseTextDecoration(_ value: String, into config: inout TextDecorationConfig) {
        let parts = value.lowercased().split(separator: " ").map { String($0) }

        // Positional parsing: parts[0]=line, parts[1]=style, parts[2]=color
        if parts.count >= 1, let line = TextDecorationLine(rawValue: parts[0]) {
            config.line = line
        }
        if parts.count >= 2, let style = TextDecorationStyle(rawValue: parts[1]) {
            config.style = style
        }
        if parts.count >= 3 {
            let parsedColor = CSSPropertyParser.parseColor(parts[2])
            if case .color(let colorValue) = parsedColor {
                config.color = colorValue
            }
        }
    }
    
    /// Parse length value (supports px, em, etc.)
    private static func parseLength(_ value: String) -> CGFloat? {
        let cleanValue = value.replacingOccurrences(of: "px", with: "")
            .replacingOccurrences(of: "em", with: "")
            .trimmingCharacters(in: .whitespaces)
        
        if let number = Double(cleanValue) {
            return CGFloat(number) * Component.BS_POINT_SCALE
        }
        return nil
    }
}

// MARK: - Text Decoration Label

/// UILabel subclass supporting W3C standard text decorations
class TextDecorationLabel: UILabel {
    
    /// Text decoration configuration
    var decorationConfig: TextDecorationConfig? {
        didSet {
            updateTextDecoration()
        }
    }
    
    override var text: String? {
        didSet {
            updateTextDecoration()
        }
    }
    
    override var attributedText: NSAttributedString? {
        didSet {
            if decorationConfig != nil {
                updateTextDecoration()
            }
        }
    }
    
    /// Update text decoration
    private func updateTextDecoration() {
        guard let config = decorationConfig,
              config.line != .none,
              let currentText = text,
              !currentText.isEmpty else {
            return
        }
        
        // Create attributed string
        let attributes = createDecorationAttributes(config: config)
        let attributedString = NSMutableAttributedString(string: currentText)
        
        // Use NSString length to ensure correct NSRange for Unicode characters like emoji
        let fullRange = NSRange(location: 0, length: (currentText as NSString).length)
        
        // Preserve existing font and color attributes
        if let existingFont = font {
            attributedString.addAttribute(.font, value: existingFont, range: fullRange)
        }
        if let existingColor = textColor {
            attributedString.addAttribute(.foregroundColor, value: existingColor, range: fullRange)
        }
        
        // Preserve existing paragraph style (e.g., line-height and alignment)
        let paragraphStyle: NSMutableParagraphStyle
        if let existingAttributedText = super.attributedText,
           existingAttributedText.length > 0,
           let existingStyle = existingAttributedText.attribute(.paragraphStyle, at: 0, effectiveRange: nil) as? NSParagraphStyle,
           let mutableStyle = existingStyle.mutableCopy() as? NSMutableParagraphStyle {
            paragraphStyle = mutableStyle
        } else {
            paragraphStyle = NSMutableParagraphStyle()
            // Keep current alignment
            paragraphStyle.alignment = textAlignment
        }
        
        attributedString.addAttribute(.paragraphStyle, value: paragraphStyle, range: fullRange)
        
        // Add decoration attributes
        for (key, value) in attributes {
            attributedString.addAttribute(key, value: value, range: fullRange)
        }
        
        super.attributedText = attributedString
    }
    
    /// Create decoration attributes
    private func createDecorationAttributes(config: TextDecorationConfig) -> [NSAttributedString.Key: Any] {
        // .dashed uses custom Core Graphics drawing, no NSUnderlineStyle set
        if config.style == .dashed && config.line != .none {
            return [.customDecoration: TextDecorationConfigBox(config)]
        }

        var attributes: [NSAttributedString.Key: Any] = [:]
        
        // Set decoration line style
        var underlineStyle: NSUnderlineStyle = []
        
        switch config.style {
        case .solid:
            underlineStyle = .single
        case .double:
            underlineStyle = .double
        case .dotted:
            underlineStyle = [.single, .patternDot]
        case .dashed:
            underlineStyle = [.single, .patternDash]
        case .wavy:
            // iOS does not directly support wavy lines, use dashed instead
            underlineStyle = [.single, .patternDashDot]
        }
        
        // Adjust style based on thickness
        if config.thickness > 1.5 {
            underlineStyle.insert(.thick)
        }
        
        // Set decoration line position
        switch config.line {
        case .underline:
            attributes[.underlineStyle] = underlineStyle.rawValue
            attributes[.underlineColor] = config.color
        case .lineThrough:
            attributes[.strikethroughStyle] = underlineStyle.rawValue
            attributes[.strikethroughColor] = config.color
        case .none:
            break
        }
        
        return attributes
    }

    // MARK: - Custom Dashed Underline Drawing

    override func draw(_ rect: CGRect) {
        super.draw(rect)
        drawDashedUnderlines(in: rect)
    }

    private func drawDashedUnderlines(in rect: CGRect) {
        guard let attributedText = attributedText,
              attributedText.length > 0 else { return }
        guard let context = UIGraphicsGetCurrentContext() else { return }

        // Find decoration config early — bail out if not dashed
        guard let box = attributedText.attribute(.customDecoration, at: 0, effectiveRange: nil) as? TextDecorationConfigBox else { return }
        let config = box.config
        guard config.style == .dashed else { return }

        // Use NSLayoutManager to get per-line geometry directly.
        // Each lineFragmentRect already contains the correct Y origin and height
        // as computed by TextKit, so we don't need to manually derive perLineHeight,
        // lineCount, or verticalOffset.
        let lm = NSLayoutManager()
        let tc = NSTextContainer(size: CGSize(width: self.bounds.width, height: .greatestFiniteMagnitude))
        tc.lineFragmentPadding = 0
        tc.maximumNumberOfLines = numberOfLines
        tc.lineBreakMode = .byWordWrapping

        let fixedString = NSMutableAttributedString(attributedString: attributedText)
        let fixRange = NSRange(location: 0, length: fixedString.length)
        fixedString.enumerateAttribute(.paragraphStyle, in: fixRange, options: []) { value, range, _ in
            if let ps = value as? NSParagraphStyle {
                if let mutable = ps.mutableCopy() as? NSMutableParagraphStyle {
                    mutable.lineBreakMode = .byWordWrapping
                    fixedString.addAttribute(.paragraphStyle, value: mutable, range: range)
                }
            }
        }
        let ts = NSTextStorage(attributedString: fixedString)
        ts.addLayoutManager(lm)
        lm.addTextContainer(tc)
        lm.ensureLayout(for: tc)
        let glyphRange = lm.glyphRange(for: tc)

        var lineInfos: [(lineRect: CGRect, usedRect: CGRect, textRect: CGRect)] = []
        lm.enumerateLineFragments(forGlyphRange: glyphRange) {
            lineFragmentRect, usedRect, container, lineGlyphRange, _ in
            let textRect = lm.boundingRect(forGlyphRange: lineGlyphRange, in: container)
            lineInfos.append((lineFragmentRect, usedRect, textRect))
        }

        guard !lineInfos.isEmpty else { return }

        let font = attributedText.attribute(.font, at: 0, effectiveRange: nil) as? UIFont
        // Read baselineOffset directly from attributed string (set by buildAttributedText
        // as half-leading = (targetLineHeight - font.lineHeight) / 2).
        // No need to manually compute halfLeading from perLineHeight.
        let baselineOffset = (attributedText.attribute(.baselineOffset, at: 0, effectiveRange: nil) as? CGFloat) ?? 0
        let color = (config.color != nil) ? config.color : textColor
        let dashWidth = config.dashWidth
        let dashGap = config.dashGap
        let underlineHeight = config.thickness
        let underlineOffset = config.offset
        let strokeHalf = underlineHeight / 2.0

        let defaultOffset: CGFloat = {
            let base = underlineHeight * 2.0
            if let font = font { return max(base, abs(font.descender) - strokeHalf) }
            return base
        }()
        // Draw one dashed decoration per line fragment from NSLayoutManager.
        // lineFragmentRect already has the correct Y origin and height from TextKit,
        // and baselineOffset encodes the half-leading centering — no manual perLineHeight needed.
        for (_, info) in lineInfos.enumerated() {
            let lineRect = info.lineRect
            let lineTop = lineRect.minY
            let lineBottom = lineRect.maxY
            let lineHeight = lineRect.height

            // Compute Y offset relative to baseline based on decoration line position:
            //   .underline   — below baseline (default)
            //   .lineThrough — at x-height center (~1/3 of ascender above baseline)
            let lineYOffset: CGFloat
            switch config.line {
            case .underline:
                lineYOffset = defaultOffset + underlineOffset
            case .lineThrough:
                // Strikethrough sits at roughly 1/3 of ascender height above baseline
                lineYOffset = -(font?.ascender ?? lineHeight) * 0.3
            case .none:
                lineYOffset = 0
            }
            // Baseline position within the line fragment:
            //   lineTop + ascender (natural baseline) + baselineOffset (half-leading shift)
            let baselineY = lineTop + (font?.ascender ?? lineHeight * 0.8) + baselineOffset
            let y = baselineY + lineYOffset

            // Cap Y to stay within the line fragment
            let descenderBottom = baselineY + abs(font?.descender ?? 0)
            let maxY = min(lineBottom - strokeHalf, descenderBottom + 1.0)
            let minY = lineTop + strokeHalf
            let cappedY = min(max(y, minY), maxY)

            // X: use per-line usedRect (the actual used portion of each line fragment)
            // instead of boundingRect which can overshoot with negative baselineOffset.
            let drawMinX = max(0, info.usedRect.minX)
            let drawMaxX = min(self.bounds.width, info.usedRect.maxX)

            // Skip if outside the dirty rect
            if !CGRect(x: drawMinX - 1, y: lineTop - 1, width: drawMaxX - drawMinX + 2, height: lineHeight + 2).intersects(rect) {
                continue
            }

            context.saveGState()
            context.setStrokeColor(color?.cgColor ?? UIColor.black.cgColor)
            context.setLineWidth(underlineHeight)
            context.setLineCap(.butt)
            context.setLineDash(phase: 0, lengths: [dashWidth, dashGap])
            context.move(to: CGPoint(x: drawMinX, y: cappedY))
            context.addLine(to: CGPoint(x: drawMaxX, y: cappedY))
            context.strokePath()
            context.restoreGState()
        }

    }
}

/// Line height type
enum LineHeightType {
    case multiplier(CGFloat)  // Numeric multiplier, e.g., 1.5
    case absolute(CGFloat)    // Absolute line height (px), e.g., 10.0
}

enum TextAlignment: Int {
    case leftTop = 0       /// Left top
    case leftCenter = 1    /// Left center
    case leftBottom = 2    /// Left bottom
    case centerTop = 3     /// Center top
    case center = 4        /// Center (horizontal + vertical)
    case centerBottom = 5  /// Center bottom
    case rightTop = 6      /// Right top
    case rightCenter = 7   /// Right center
    case rightBottom = 8   /// Right bottom
}

extension TextAlignment {
    
    init?(normalizedString: String) {
        // Normalize input
        let key = normalizedString
            .lowercased()
            .replacingOccurrences(of: " ", with: "")
            .replacingOccurrences(of: "-", with: "")
            .trimmingCharacters(in: .whitespaces)
        
        // Predefined mapping dictionary (static, avoid repeated creation)
        let mapping: [String: TextAlignment] = [
            "left": .leftCenter,
            "lefttop": .leftTop,
            "leftcenter": .leftCenter,
            "leftbottom": .leftBottom,
            "centertop": .centerTop,
            "center": .center,          // Note: standalone "center" maps to .center
            "centercenter": .center,
            "centerbottom": .centerBottom,
            "right": .rightCenter,
            "righttop": .rightTop,
            "rightcenter": .rightCenter,
            "rightbottom": .rightBottom
        ]
        
        guard let value = mapping[key] else {
            return nil // Invalid input: initialization failed
        }
        self = value // Safe assignment of non-optional value
    }
    
}

/// TextComponent component implementation
///
/// Supported properties:
/// - text: Text content string (String)
/// - textChunk: Content to append to existing text (String)
/// - variant: Text style hint (String: h1, h2, h3, h4, h5, caption, body)
/// - styles: CSS style dictionary containing:
///   - font-size: Font size with optional px unit (String)
///   - font-weight: Font weight (String: bold, normal, light, thin or numeric 100-700)
///   - font-family: Font family name (String)
///   - color: Text color in hex or named format (String)
///   - text-align: Text alignment (String: left, center, right)
///   - line-height: Line height as multiplier or px value (String/Double/Int)
///   - line-clamp: Maximum number of lines (Int/String)
///   - text-overflow: Overflow behavior (String: ellipsis, clip, head, middle)
///   - text-decoration: Text decoration shorthand (String)
///   - text-decoration-line: Decoration line (String: none, underline, line-through)
///   - text-decoration-style: Decoration style (String: solid, double, dotted, dashed, wavy)
///   - text-decoration-color: Decoration color (String)
///   - text-decoration-thickness: Decoration thickness (String/CGFloat)
///
/// Design notes:
/// - Uses TextDecorationLabel (custom UILabel subclass) for W3C-compliant text decorations
/// - Applies a "collect, synthesize, then apply" architecture to avoid property overwriting
/// - Converts px units using BS_POINT_SCALE factor (0.5) for font-size and line-height
/// - Supports Unicode/emoji text via NSString-based NSRange calculations
class TextComponent: Component {
    
    // MARK: - Constants
    
    // MARK: - Properties
    
    private var label: TextDecorationLabel?

    // Label-to-self edge constraints. Mutated by `applyTextPadding` so that CSS
    // `padding` values shrink the glyph rendering area (TextView/UILabel
    // equivalent of TextView.setPadding on Android, since UILabel itself does
    // not natively support padding).
    //
    // No bottom constraint: label height is driven by intrinsicContentSize
    // (numberOfLines=0 + fixed width). clipsToBounds=false on self allows
    // text to overflow past the bottom edge (W3C overflow-visible).
    private var labelTopConstraint: NSLayoutConstraint?
    private var labelLeadingConstraint: NSLayoutConstraint?
    private var labelTrailingConstraint: NSLayoutConstraint?
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        // Component itself is a UIView
        super.init(componentId: componentId, componentType: "Text", properties: properties)
        
        // Create label
        let label = TextDecorationLabel()
        label.numberOfLines = 0
        label.lineBreakMode = .byWordWrapping
        label.font = UIFont.systemFont(ofSize: 16)
        label.textColor = .black
        
        // Add subview with AutoLayout constraints to fill parent.
        // Constraints are stored as members so `applyTextPadding` can mutate
        // their `constant` to honour CSS `padding`.
        // No bottom constraint: label height is driven by intrinsicContentSize
        // (numberOfLines=0 + fixed width from trailingC).
        label.translatesAutoresizingMaskIntoConstraints = false
        addSubview(label)
        // clipsToBounds=false allows text to overflow past self's bottom
        // edge when content exceeds the Yoga border-box height.
        self.clipsToBounds = false
        let topC = label.topAnchor.constraint(equalTo: topAnchor)
        let leadingC = label.leadingAnchor.constraint(equalTo: leadingAnchor)
        let trailingC = label.trailingAnchor.constraint(equalTo: trailingAnchor)
        NSLayoutConstraint.activate([topC, leadingC, trailingC])
        self.label = label
        self.labelTopConstraint = topC
        self.labelLeadingConstraint = leadingC
        self.labelTrailingConstraint = trailingC
        
        // Apply initial properties after label is created
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
        
    override var frame: CGRect {
        get { return super.frame }
        set {
            // Check if height is abnormal
            if newValue.height > 1000 {
                Logger.shared.warning("[TextDecorationLabel] Abnormal height detected: \(newValue.height), frame: \(newValue))")
            }
            super.frame = newValue
        }
    }
    
    // MARK: - Component Override
    
    override func updateProperties(_ properties: [String: Any]) {
        // Call parent method to apply CSS properties to self (padding, background-color, etc.)
        super.updateProperties(properties)

        guard let label = label else { return }

        // Handle textChunk field (content append, supports both String and numeric types)
        if let textChunkValue = properties["textChunk"] {
            let textChunk = TextComponent.extractTextValue(textChunkValue) ?? ""
            if !textChunk.isEmpty {
                let currentText = label.text ?? ""
                label.text = currentText + textChunk
                label.invalidateIntrinsicContentSize()
            }
        }
        // Update text content (supports both String and numeric types)
        else if let textValue = properties["text"] {
            let text = TextComponent.extractTextValue(textValue) ?? ""
            label.text = text.count == 0 ? " " : text
            label.invalidateIntrinsicContentSize()
        }

        // Update style properties
        if let styles = properties["styles"] as? [String: Any] {
            applyStyles(styles)
        }
    }
    
    // MARK: - Parsed Text Styles (shared intermediate representation)

    /// Parsed text style properties (intermediate representation)
    /// Shared by applyStyles and measure to ensure consistent parsing
    private struct ParsedTextStyles {
        var fontSize: CGFloat?
        var fontWeight: UIFont.Weight?
        var fontFamily: String?
        var color: UIColor?
        var textAlign: NSTextAlignment?
        var lineHeight: LineHeightType?
        var lineClamp: Int?
        var textOverflow: String?
        var decorationConfig: TextDecorationConfig?
    }

    /// Parse styles dictionary into intermediate representation
    /// Shared by applyStyles and measure to ensure consistent parsing logic
    private class func parseStyles(_ styles: [String: Any]) -> ParsedTextStyles {
        var parsed = ParsedTextStyles()

        // Parse font-size (supports String with px unit and NSNumber)
        if let fontSizeValue = styles["font-size"] as? String {
            parsed.fontSize = extractFontSize(from: fontSizeValue)
        } else if let num = styles["font-size"] as? NSNumber {
            parsed.fontSize = CGFloat(num.doubleValue) * Component.BS_POINT_SCALE
        }

        // Parse font-weight
        if let fontWeightValue = styles["font-weight"] as? String {
            parsed.fontWeight = ComponentStyleConfigManager.parseFontWeight(fontWeightValue)
        }
        else if let num = styles["font-weight"] as? NSNumber {
            parsed.fontWeight = ComponentStyleConfigManager.parseFontWeight(num.stringValue)
        }

        // Parse font-family
        if let fontFamilyValue = styles["font-family"] as? String {
            parsed.fontFamily = fontFamilyValue
        }

        // Parse color
        if let colorValue = styles["color"] as? String {
            let parsedColor = CSSPropertyParser.parseColor(colorValue)
            if case .color(let value) = parsedColor {
                parsed.color = value
            }
        }

        // Parse text-align
        if let textAlignValue = styles["text-align"] as? String {
            parsed.textAlign = parseTextAlign(textAlignValue)
        }

        // Parse line-height (compatible with string and number)
        if let lineHeightValue = styles["line-height"] {
            parsed.lineHeight = extractLineHeight(from: lineHeightValue)
        }

        // Parse line-clamp (compatible with string and number)
        if let lineClampValue = styles["line-clamp"] {
            if let intValue = lineClampValue as? Int {
                parsed.lineClamp = intValue
            } else if let stringValue = lineClampValue as? String, let parsedValue = Int(stringValue) {
                parsed.lineClamp = parsedValue
            }
        }

        // Parse text-overflow
        if let textOverflowValue = styles["text-overflow"] as? String {
            parsed.textOverflow = textOverflowValue
        }

        // Parse text-decoration
        parsed.decorationConfig = TextDecorationConfig.from(styles: styles)

        return parsed
    }

    // MARK: - Private Methods

    /// Apply style properties
    ///
    /// Uses a 'collect, synthesize, then apply' architecture to avoid property overwriting:
    /// - Phase 1: Parse all styles to intermediate variables, do not touch label
    /// - Phase 2: Synthesize UIFont with family + weight + size in one pass
    /// - Phase 3: Synthesize NSAttributedString with font + color + lineHeight + align + decoration in one pass
    /// - Phase 4: Apply non-text properties (lineClamp, textOverflow)
    private func applyStyles(_ styles: [String: Any]) {
        guard let label = label else { return }

        // ========================
        // Phase 1: Collect - parse all styles to intermediate variables (reuse parseStyles)
        // ========================
        let parsed = TextComponent.parseStyles(styles)

        // ========================
        // Phase 2: Synthesize UIFont - family + weight + size one-time build
        // ========================

        let currentFont = label.font ?? UIFont.systemFont(ofSize: 16)
        let finalSize = parsed.fontSize ?? currentFont.pointSize
        let finalWeight = parsed.fontWeight ?? currentFontWeight(from: currentFont)

        let finalFont: UIFont
        if let family = parsed.fontFamily {
            finalFont = TextComponent.buildFont(family: family, weight: finalWeight, size: finalSize)
        } else if parsed.fontSize != nil || parsed.fontWeight != nil {
            // size or weight changed, but family not specified, keep current family
            let currentFamily = currentFont.familyName
            finalFont = TextComponent.buildFont(family: currentFamily, weight: finalWeight, size: finalSize)
        } else {
            finalFont = currentFont
        }

        label.font = finalFont

        // ========================
        // Phase 3: Synthesize NSAttributedString - build all text attributes in one pass
        // ========================

        let finalColor = parsed.color ?? label.textColor ?? .black
        label.textColor = finalColor

        // text-align set directly on label (consistent with original behavior, does not trigger attributedText creation)
        if let textAlign = parsed.textAlign {
            label.textAlignment = textAlign
        }

        label.numberOfLines = parsed.lineClamp ?? 0

        // Only create attributedText when lineHeight or decoration exists
        let needsAttributedText = parsed.lineHeight != nil
            || parsed.decorationConfig != nil

        if needsAttributedText {
            let currentText = label.text ?? ""
            if let attributedString = TextComponent.buildAttributedText(
                text: currentText,
                font: finalFont,
                color: finalColor,
                textAlignment: label.textAlignment,
                lineHeight: parsed.lineHeight,
                lineClamp: parsed.lineClamp ?? 0,
                decorationConfig: parsed.decorationConfig
            ) {
                label.attributedText = attributedString
            }
        }

        if let textOverflow = parsed.textOverflow {
            applyTextOverflow(to: label, overflow: textOverflow)
        }

        // Apply CSS padding to the inner label.
        //
        // Why this is needed even though Yoga already accounts for padding:
        // The C++ Yoga engine sets `YGNodeStyleSetPadding` on the leaf node,
        // so the TextComponent's own frame is the borderBox (content + padding)
        // and `measure` receives the contentBox. But `label` is a subview
        // pinned with constant=0 anchor constraints, which makes it fill the
        // borderBox and lets the glyphs paint over what should be the padded
        // region. Translating CSS padding to the four edge constants shrinks
        // the label down to the contentBox, matching Android's
        // `TextView.setPadding(...)` behaviour and Harmony's
        // `BaseNode.setPadding(...)`. setPadding-style updates do NOT affect
        // self.frame, so this is not double-counted with Yoga.
        applyTextPadding(styles)
    }

    /// Translate CSS `padding` (and the four single-edge overrides) into the
    /// `constant` of the four label edge constraints. Supports the W3C 1/2/3/4
    /// component shorthand and `padding-top/right/bottom/left` overrides.
    /// Parsing is delegated to the shared `CSSPaddingResolver` so all
    /// leaf-style components share a single implementation.
    private func applyTextPadding(_ styles: [String: Any]) {
        guard let topC = labelTopConstraint,
              let leadingC = labelLeadingConstraint,
              let trailingC = labelTrailingConstraint else { return }

        // Skip silently if the styles dict carries no padding-* key, so we
        // do not clobber existing constants.
        guard CSSPaddingResolver.hasAnyPaddingKey(styles) else { return }

        let p = CSSPaddingResolver.resolve(styles)
        topC.constant = p.top
        leadingC.constant = p.left
        // trailing anchor is pinned with `equalTo: parent`, so a positive
        // inset is encoded as a NEGATIVE constant.
        trailingC.constant = -p.right
        // No bottom constraint: paddingBottom is handled by Yoga's
        // border-box frame; label height is driven by intrinsicContentSize.
    }

    /// Count the number of line fragments produced when laying out `attributedString`
    /// in a container of width `width`. Mirrors UILabel's TextKit-based wrapping.
    private class func countLineFragments(attributedString: NSAttributedString,
                                          width: CGFloat) -> Int {
        let manager = NSLayoutManager()
        let storage = NSTextStorage(attributedString: attributedString)
        let container = NSTextContainer(size: CGSize(width: width, height: .greatestFiniteMagnitude))
        container.lineFragmentPadding = 0
        container.lineBreakMode = .byWordWrapping
        container.maximumNumberOfLines = 0
        storage.addLayoutManager(manager)
        manager.addTextContainer(container)
        manager.ensureLayout(for: container)

        var count = 0
        var index = 0
        let glyphCount = manager.numberOfGlyphs
        while index < glyphCount {
            var range = NSRange()
            let fragRect = manager.lineFragmentRect(forGlyphAt: index, effectiveRange: &range)
            index = NSMaxRange(range)
            count += 1
        }
        return count
    }
    
    // MARK: - Text Value Extraction

    /// Extract text value from Any type, supporting both String and numeric types (Int, Double, NSNumber)
    /// Numbers are converted to their string representation via `description`.
    private class func extractTextValue(_ value: Any?) -> String? {
        guard let value = value else { return nil }
        if let stringValue = value as? String {
            return stringValue
        }
        if let numberValue = value as? NSNumber {
            return "\(numberValue)"
        }
        return nil
    }

    // MARK: - Phase 1 Helpers: Pure parsing, no view state modification
    
    /// Parse text-align string to NSTextAlignment
    private class func parseTextAlign(_ alignment: String) -> NSTextAlignment? {
        guard let textAlignment = TextAlignment(normalizedString: alignment) else { return nil }
        
        switch textAlignment {
        case .leftTop, .leftCenter, .leftBottom:
            return .left
        case .centerTop, .center, .centerBottom:
            return .center
        case .rightTop, .rightCenter, .rightBottom:
            return .right
        }
    }
    
    // MARK: - Phase 2 Helpers: UIFont synthesis
    
    /// Extract UIFont.Weight from current UIFont
    private func currentFontWeight(from font: UIFont) -> UIFont.Weight {
        let traits = font.fontDescriptor.object(forKey: .traits) as? [UIFontDescriptor.TraitKey: Any]
        if let weightValue = traits?[.weight] as? CGFloat {
            return UIFont.Weight(rawValue: weightValue)
        }
        return .regular
    }
    
    /// Build UIFont with family + weight + size in one pass
    private class func buildFont(family: String, weight: UIFont.Weight, size: CGFloat) -> UIFont {
        // CSS fallback list: "CustomFont, monospace, sans-serif"
        let candidates = family.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }

        for candidate in candidates {
            let name = stripFontQuotes(candidate)
            if name.isEmpty { continue }

            if let resolved = resolveOneFamily(name, weight: weight, size: size) {
                return resolved
            }
        }

        return UIFont.systemFont(ofSize: size, weight: weight)
    }

    private class func stripFontQuotes(_ value: String) -> String {
        if value.count >= 2 {
            let first = value.first!, last = value.last!
            if (first == "\"" && last == "\"") || (first == "'" && last == "'") {
                return String(value.dropFirst().dropLast())
            }
        }
        return value
    }

    private class func resolveOneFamily(_ name: String, weight: UIFont.Weight, size: CGFloat) -> UIFont? {
        let lower = name.lowercased()

        // Generic family names
        switch lower {
        case "system", "sans-serif":
            return UIFont.systemFont(ofSize: size, weight: weight)
        case "monospace":
            return UIFont.monospacedSystemFont(ofSize: size, weight: weight)
        case "serif":
            if let serifDesc = UIFontDescriptor.preferredFontDescriptor(withTextStyle: .body).withDesign(.serif) {
                let weighted = serifDesc.addingAttributes([
                    .traits: [UIFontDescriptor.TraitKey.weight: weight.rawValue]
                ])
                return UIFont(descriptor: weighted, size: size)
            }
            return UIFont(name: "Times New Roman", size: size) ?? UIFont.systemFont(ofSize: size, weight: weight)
        default:
            break
        }

        // FontRegistry lookup
        let resolvedName: String
        if let registered = FontRegistry.shared.resolve(familyName: name) {
            resolvedName = registered
        } else {
            resolvedName = name
        }

        // Try UIFontDescriptor (matches by family name)
        let descriptor = UIFontDescriptor(fontAttributes: [
            .family: resolvedName,
            .traits: [UIFontDescriptor.TraitKey.weight: weight.rawValue]
        ])
        let font = UIFont(descriptor: descriptor, size: size)
        if font.familyName.lowercased() == resolvedName.lowercased() {
            return font
        }

        // Try exact font name (PostScript name)
        if let exactFont = UIFont(name: resolvedName, size: size) {
            return exactFont
        }

        return nil
    }
    
    // MARK: - Phase 3 Helpers: NSAttributedString synthesis
    
    /// Build text decoration attribute dictionary (does not modify label)
    private class func buildDecorationAttributes(config: TextDecorationConfig) -> [NSAttributedString.Key: Any] {
        // .dashed uses custom Core Graphics drawing, no NSUnderlineStyle set
        if config.style == .dashed && config.line != .none {
            return [.customDecoration: TextDecorationConfigBox(config)]
        }

        var attributes: [NSAttributedString.Key: Any] = [:]
        
        var underlineStyle: NSUnderlineStyle = []
        
        switch config.style {
        case .solid:
            underlineStyle = .single
        case .double:
            underlineStyle = .double
        case .dotted:
            underlineStyle = [.single, .patternDot]
        case .dashed:
            underlineStyle = [.single, .patternDash]
        case .wavy:
            underlineStyle = [.single, .patternDashDot]
        }
        
        if config.thickness > 1.5 {
            underlineStyle.insert(.thick)
        }
        
        switch config.line {
        case .underline:
            attributes[.underlineStyle] = underlineStyle.rawValue
            attributes[.underlineColor] = config.color
        case .lineThrough:
            attributes[.strikethroughStyle] = underlineStyle.rawValue
            attributes[.strikethroughColor] = config.color
        case .none:
            break
        }
        
        return attributes
    }
    
    /// Extract font size from string
    private class func extractFontSize(from fontSizeString: String) -> CGFloat? {
        if fontSizeString.hasSuffix("px") {
            // px unit: apply BS_POINT_SCALE scaling
            // Formula: xpx * BS_POINT_SCALE
            // Example: "32px" = 32 * 0.5 = 16.0
            let cleanString = fontSizeString.replacingOccurrences(of: "px", with: "")
            if let size = Double(cleanString) {
                return CGFloat(size) * Component.BS_POINT_SCALE
            }
        } else {
            // No unit or other unit: use value directly
            if let size = Double(fontSizeString) {
                return CGFloat(size) * Component.BS_POINT_SCALE
            }
        }
        return nil
    }
    
    /// Extract line height value from multiple types (compatible with String, Double, Int)
    /// Supports two formats:
    /// - Numeric multiplier (Double/Int/no-unit string): returns .multiplier, e.g., 0.5, 1.5
    /// - px value (with px unit string): returns .absolute, applies BS_POINT_SCALE scaling, e.g., "10px" = 10 * 0.5 = 5.0
    private class func extractLineHeight(from value: Any?) -> LineHeightType? {
        guard let value = value else { return nil }
        
        if let doubleValue = value as? Double {
            // Numeric multiplier
            return .multiplier(CGFloat(doubleValue))
        } else if let intValue = value as? Int {
            // Numeric multiplier
            return .multiplier(CGFloat(intValue))
        } else if let stringValue = value as? String {
            if stringValue.hasSuffix("px") {
                // px unit: apply BS_POINT_SCALE scaling to absolute line height
                let cleanValue = stringValue.replacingOccurrences(of: "px", with: "")
                    .trimmingCharacters(in: .whitespaces)
                if let parsed = Double(cleanValue) {
                    return .absolute(CGFloat(parsed) * Component.BS_POINT_SCALE)
                }
            } else {
                // No-unit string: numeric multiplier
                if let parsed = Double(stringValue) {
                    return .multiplier(CGFloat(parsed))
                }
            }
        }
        return nil
    }
    
    /// Apply text overflow handling
    private func applyTextOverflow(to label: UILabel, overflow: String) {
        switch overflow.lowercased() {
        case "ellipsis":
            label.lineBreakMode = .byTruncatingTail
        case "clip":
            label.lineBreakMode = .byClipping
        case "head":
            label.lineBreakMode = .byTruncatingHead
        case "middle":
            label.lineBreakMode = .byTruncatingMiddle
        default:
            break
        }
    }

    // MARK: - Measurement Override

    /// Measure the intrinsic size of the text component
    override class func measure(type: String,
                                paramJson: String,
                                maxWidth: Float,
                                widthMode: MeasureMode,
                                maxHeight: Float,
                                heightMode: MeasureMode) -> CGSize {
        // 1. Parse paramJson
        guard let jsonData = paramJson.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            return .zero
        }

        // 2. Extract text content (supports both String and numeric types)
        let text = extractTextValue(json["text"]) ?? ""
      
        // 3. Parse styles (reuse same parsing as applyStyles Phase 1)
        let styles = json["styles"] as? [String: Any] ?? [:]
        let parsed = parseStyles(styles)

        // 4. Build UIFont (reuse same logic as applyStyles Phase 2)
        let fontSize = parsed.fontSize ?? 16.0
        let fontWeight = parsed.fontWeight ?? .regular
        let fontFamily = parsed.fontFamily ?? "system"
        let font = buildFont(family: fontFamily, weight: fontWeight, size: fontSize)

        // 5. Build NSAttributedString (reuse same logic as applyStyles Phase 3)
        let textAlignment = parsed.textAlign ?? .left
        let lineClamp = parsed.lineClamp ?? 0

        guard let attributedString = buildAttributedText(
            text: text,
            font: font,
            color: .black,
            textAlignment: textAlignment,
            lineHeight: parsed.lineHeight,
            lineClamp: lineClamp,
            decorationConfig: parsed.decorationConfig
        ) else {
            return .zero
        }

        // 6. Calculate boundingRect
        // For Exactly/AtMost, constrain text wrapping to maxWidth; for Undefined, let text expand freely
        let constraintWidth: CGFloat = (widthMode == .undefined) ? .greatestFiniteMagnitude : CGFloat(maxWidth)
        let bounds = attributedString.boundingRect(
            with: CGSize(width: constraintWidth, height: .greatestFiniteMagnitude),
            options: [.usesLineFragmentOrigin, .usesFontLeading],
            context: nil)

        var measuredWidth = bounds.size.width
        var measuredHeight = bounds.size.height

        // When a CSS line-height is specified, the rendered line-box height equals
        // `targetLineHeight` (the same value the renderer applies via `minimumLineHeight`/
        // `maximumLineHeight` in `buildAttributedText`), but `boundingRect` reports
        // `font.lineHeight + baselineOffset` per line — slightly smaller. If we feed that
        // shorter height back to Yoga, the frame ends up just below `lineCount × targetLineHeight`,
        // and `UILabel` then drops the bottom line wholesale instead of rendering a partial pixel.
        // Recompute the measured height from the actual line count × `targetLineHeight` so the
        // measure value matches the renderer's line box and every wrapped line is visible.
        if let lineHeight = parsed.lineHeight {
            let perLineHeight: CGFloat
            switch lineHeight {
            case .multiplier(let value):
                perLineHeight = font.pointSize * value
            case .absolute(let value):
                perLineHeight = value
            }
            let lineCount = countLineFragments(
                attributedString: attributedString,
                width: constraintWidth)
            measuredHeight = ceil(perLineHeight * CGFloat(max(lineCount, 1)))
        }

        // 7. Apply lineClamp height cap
        // boundingRect always measures all lines; when lineClamp > 0, cap height to lineClamp lines.
        // The per-line height here must stay in lockstep with `buildAttributedText`, which now
        // applies a centered line box (`minimumLineHeight = maximumLineHeight = targetLineHeight`)
        // for both single-line and multi-line text. Therefore the total clamped height is simply
        // `N * targetLineHeight` with zero inter-line spacing.
        if lineClamp > 0 {
            let perLineHeight: CGFloat
            if let lineHeight = parsed.lineHeight {
                switch lineHeight {
                case .multiplier(let value):
                    perLineHeight = font.pointSize * value
                case .absolute(let value):
                    perLineHeight = value
                }
            } else {
                perLineHeight = font.lineHeight
            }
            let maxClampedHeight = ceil(perLineHeight * CGFloat(lineClamp))
            measuredHeight = min(measuredHeight, maxClampedHeight)
        }

        // 8. Apply MeasureMode constraints
        if (widthMode == .exactly || widthMode == .atMost) && maxWidth > 0 {
            measuredWidth = widthMode == .atMost
                ? min(measuredWidth, CGFloat(maxWidth))
                : CGFloat(maxWidth)
        }
        if (heightMode == .exactly || heightMode == .atMost) && maxHeight > 0 {
            measuredHeight = heightMode == .atMost
                ? min(measuredHeight, CGFloat(maxHeight))
                : CGFloat(maxHeight)
        }

        let result = CGSize(width: measuredWidth, height: measuredHeight)
        return result
    }

    // MARK: - Shared Text Construction (used by both applyStyles and measure)

    /// Build NSAttributedString from parsed style parameters
    /// Shared by applyStyles (Phase 3) and measure to ensure consistent text rendering
    private class func buildAttributedText(
        text: String,
        font: UIFont,
        color: UIColor,
        textAlignment: NSTextAlignment,
        lineHeight: LineHeightType?,
        lineClamp: Int,
        decorationConfig: TextDecorationConfig?
    ) -> NSAttributedString? {
        guard !text.isEmpty else { return nil }

        let attributedString = NSMutableAttributedString(string: text)
        let fullRange = NSRange(location: 0, length: (text as NSString).length)

        // Set font
        attributedString.addAttribute(.font, value: font, range: fullRange)

        // Set color
        attributedString.addAttribute(.foregroundColor, value: color, range: fullRange)

        // Synthesize paragraph style
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.alignment = textAlignment

        if let lineHeight = lineHeight {
            // W3C line-height semantics: the line box height equals `multiplier * font-size`
            // (or the absolute px value). The glyph content-area is vertically centered inside
            // the line box, so the first line has a half-leading gap above and the last line
            // has a half-leading gap below. This matches Harmony (ArkUI `NODE_TEXT_LINE_HEIGHT`)
            // and the Android `CenteredLineHeightSpan` implementation.
            //
            // Previously iOS only centered single-line text and fell back to
            // `paragraphStyle.lineSpacing = (N-1) * font.pointSize` for multi-line, which piles
            // all extra space between lines and leaves the glyphs flush with the line-box top.
            // Unify single- and multi-line through `minimumLineHeight`/`maximumLineHeight` plus
            // a `baselineOffset` nudge so every line is centered in its line box.
            let defaultLineHeight = font.lineHeight
            let targetLineHeight: CGFloat
            switch lineHeight {
            case .multiplier(let value):
                targetLineHeight = font.pointSize * value
            case .absolute(let value):
                targetLineHeight = value
            }

            paragraphStyle.minimumLineHeight = targetLineHeight
            paragraphStyle.maximumLineHeight = targetLineHeight
            let baselineOffset = (targetLineHeight - defaultLineHeight) / 2
            attributedString.addAttribute(.baselineOffset, value: baselineOffset, range: fullRange)
        }

        attributedString.addAttribute(.paragraphStyle, value: paragraphStyle, range: fullRange)

        // Set text decoration
        if let config = decorationConfig, config.line != .none {
            let decorationAttrs = buildDecorationAttributes(config: config)
            for (key, value) in decorationAttrs {
                attributedString.addAttribute(key, value: value, range: fullRange)
            }
        }

        return attributedString
    }
}

