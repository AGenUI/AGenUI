//
//  RichTextComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

#if AGENUI_SDK_BUILD
import UIKit

/// RichText component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - text: Rich text content supporting HTML format (String, required)
/// - linksEnable: Whether to enable link clicks (Boolean, default true)
/// - styles: Style dictionary including font-size, color, text-align, etc. (Dictionary)
///
/// Design notes:
/// - Parses HTML to NSAttributedString with support for text formatting, headings, lists, links, and images
/// - Supports image scaling based on line height for inline rendering
/// - Link tap handling via UITapGestureRecognizer with NSLayoutManager for character index detection
class RichTextComponent: Component {
    
    // MARK: - Properties
    
    private var label: UILabel?
    private var linkRanges: [(range: NSRange, url: String)] = []
    private var linksEnable: Bool = true  // Default enable link clicks
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "RichText", properties: properties)
        
        // Create label and add to self
        let label = UILabel()
        label.numberOfLines = 0  // Support multi-line
        label.isUserInteractionEnabled = true  // Default support interaction
        
        addSubview(label)
        self.label = label
        
        // Add tap gesture recognizer
        let tapGesture = UITapGestureRecognizer(target: self, action: #selector(handleLabelTap(_:)))
        label.addGestureRecognizer(tapGesture)
        
        // Apply initial properties
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Measurement Override
    
    override class func measure(type: String, paramJson: String, maxWidth: Float, widthMode: MeasureMode, maxHeight: Float, heightMode: MeasureMode) -> CGSize {
        guard let jsonData = paramJson.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            return .zero
        }
        
        // Extract text content
        let contentString: String
        if let literalString = json["literalString"] as? String {
            contentString = literalString
        } else if let text = json["text"] as? String {
            contentString = text
        } else {
            return .zero
        }
        guard !contentString.isEmpty else { return .zero }
        
        let styles = json["styles"] as? [String: Any]
        guard let attrStr = buildAttributedString(htmlString: contentString, styles: styles) else {
            return .zero
        }
        
        let constraintWidth: CGFloat = (widthMode == .undefined) ? .greatestFiniteMagnitude : CGFloat(maxWidth)
        let boundingRect = attrStr.boundingRect(
            with: CGSize(width: constraintWidth, height: .greatestFiniteMagnitude),
            options: [.usesLineFragmentOrigin, .usesFontLeading],
            context: nil
        )
        
        var measuredWidth = boundingRect.width
        var measuredHeight = boundingRect.height
        
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
        
        return CGSize(width: measuredWidth, height: measuredHeight)
    }
    
    // MARK: - Layout
    
    override func layoutSubviews() {
        super.layoutSubviews()
        
        // Sync label frame with component bounds
        label?.frame = bounds
    }
    
    override func updateProperties(_ properties: [String: Any]) {
        super.updateProperties(properties)
        guard let label = label else { return }
        
        // Update linksEnable property
        if let enable = properties["linksEnable"] as? Bool {
            linksEnable = enable
            label.isUserInteractionEnabled = enable
        }
        
        // Get rich text content
        var contentString = ""
        if let literalString = properties["literalString"] as? String {
            contentString = literalString
        } else if let text = properties["text"] as? String {
            contentString = text
        }
        
        // Parse and apply rich text
        if !contentString.isEmpty {
            let attributedString = parseHTMLContentSync(contentString, styles: properties["styles"] as? [String: Any])
            label.attributedText = attributedString
            
            // Apply text alignment
            if let styles = properties["styles"] as? [String: Any],
               let textAlign = styles["text-align"] as? String {
                label.textAlignment = parseTextAlignment(textAlign)
            }
        }
    }
    
    // MARK: - Private Methods
    
    /// Async parse HTML content to NSAttributedString
    /// Parse in background thread to avoid blocking main thread
    private func parseHTMLContentAsync(_ htmlString: String, styles: [String: Any]?, completion: @escaping (NSAttributedString) -> Void) {
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self = self else { return }
            
            let attributedString = self.parseHTMLContentSync(htmlString, styles: styles)
            
            DispatchQueue.main.async {
                completion(attributedString)
            }
        }
    }
    
    /// Preprocess HTML, add scaling styles for all img tags
    private static func preprocessHTMLStatic(_ htmlString: String, maxSize: CGFloat) -> String {
        let pattern = "<img([^>]*)>"
        guard let regex = try? NSRegularExpression(pattern: pattern, options: .caseInsensitive) else {
            return htmlString
        }
        
        let range = NSRange(htmlString.startIndex..., in: htmlString)
        var processedHTML = htmlString
        var offset = 0
        let maxSizeInt = Int(maxSize)
        
        regex.enumerateMatches(in: htmlString, options: [], range: range) { match, _, _ in
            guard let match = match else { return }
            let matchRange = match.range
            let adjustedRange = NSRange(location: matchRange.location + offset, length: matchRange.length)
            
            let startIndex = String.Index(utf16Offset: adjustedRange.location, in: processedHTML)
            let endIndex = String.Index(utf16Offset: adjustedRange.location + adjustedRange.length, in: processedHTML)
            let range = startIndex..<endIndex
            let imgTag = String(processedHTML[range])
            let newTag: String
            
            // Use pixel value to limit image max width and height, keep aspect ratio
            let styleValue = "max-width: \(maxSizeInt)px; max-height: \(maxSizeInt)px; object-fit: contain; "
            
            // Check if style attribute already exists
            if imgTag.contains("style=") {
                // Add scaling style to existing style attribute
                newTag = imgTag.replacingOccurrences(
                    of: "style=\"",
                    with: "style=\"\(styleValue)"
                )
            } else {
                // Add new style attribute
                newTag = imgTag.replacingOccurrences(
                    of: "<img",
                    with: "<img style=\"\(styleValue)\""
                )
            }
            
            processedHTML.replaceSubrange(range, with: newTag)
            offset += newTag.count - imgTag.count
        }
        
        return processedHTML
    }
    
    /// Sync parse HTML content to NSAttributedString (called in background thread)
    private func parseHTMLContentSync(_ htmlString: String, styles: [String: Any]?) -> NSAttributedString {
        let attrStr = RichTextComponent.buildAttributedString(htmlString: htmlString, styles: styles)
            ?? NSAttributedString(string: htmlString)
        extractLinks(from: attrStr)
        return attrStr
    }
    
    /// Build NSAttributedString from HTML — shared by measure (class method) and parseHTMLContentSync (instance method)
    private static func buildAttributedString(htmlString: String, styles: [String: Any]?) -> NSAttributedString? {
        let fontSize: CGFloat = 16
        var textColor = "#333333"
        
        if let styles = styles {
            if let color = styles["color"] as? String {
                textColor = color
            }
        }
        
        // Calculate line height (fontSize * 1.2) as image max size
        let lineHeight = fontSize * 1.2
        
        // Preprocess HTML, add image scaling based on line height
        let preprocessedHTML = preprocessHTMLStatic(htmlString, maxSize: lineHeight)
        
        // Build HTML header, set default styles
        let htmlHead = """
        <html>
        <head>
        <style>
        body {
            font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif;
            font-size: \(fontSize)px;
            color: \(textColor);
            margin: 0;
            padding: 0;
        }
        i, em {
            font-style: italic !important;
        }
        b, strong {
            font-weight: bold !important;
        }
        </style>
        </head>
        <body>
        """
        
        let fullHTML = htmlHead + preprocessedHTML + "</body></html>"
        
        guard let data = fullHTML.data(using: .utf8) else { return nil }
        
        let options: [NSAttributedString.DocumentReadingOptionKey: Any] = [
            .documentType: NSAttributedString.DocumentType.html,
            .characterEncoding: String.Encoding.utf8.rawValue
        ]
        return try? NSAttributedString(data: data, options: options, documentAttributes: nil)
    }
    
    /// Extract link info from NSAttributedString
    private func extractLinks(from attributedString: NSAttributedString) {
        linkRanges.removeAll()
        
        attributedString.enumerateAttribute(.link, in: NSRange(location: 0, length: attributedString.length), options: []) { value, range, _ in
            if let url = value as? URL {
                linkRanges.append((range: range, url: url.absoluteString))
            } else if let urlString = value as? String {
                linkRanges.append((range: range, url: urlString))
            }
        }
    }
    
    /// Parse text alignment
    private func parseTextAlignment(_ alignment: String) -> NSTextAlignment {
        switch alignment.lowercased() {
        case "left":
            return .left
        case "center":
            return .center
        case "right":
            return .right
        case "justified":
            return .justified
        default:
            return .left
        }
    }
    
    // MARK: - Event Handling
    
    @objc private func handleLabelTap(_ gesture: UITapGestureRecognizer) {
        // If link clicks not enabled, return directly
        guard linksEnable else { return }
        
        guard let label = label,
              let attributedText = label.attributedText else {
            return
        }
        
        // Get tap location
        let location = gesture.location(in: label)
        
        // Create NSTextContainer and NSLayoutManager
        let textStorage = NSTextStorage(attributedString: attributedText)
        let layoutManager = NSLayoutManager()
        let textContainer = NSTextContainer(size: label.bounds.size)
        
        layoutManager.addTextContainer(textContainer)
        textStorage.addLayoutManager(layoutManager)
        
        textContainer.lineFragmentPadding = 0
        textContainer.maximumNumberOfLines = label.numberOfLines
        textContainer.lineBreakMode = label.lineBreakMode
        
        // Calculate character index for tap location
        let characterIndex = layoutManager.characterIndex(for: location, in: textContainer, fractionOfDistanceBetweenInsertionPoints: nil)
        
        // Check if link was clicked
        for linkInfo in linkRanges {
            if NSLocationInRange(characterIndex, linkInfo.range) {
                handleLinkClick(url: linkInfo.url)
                return
            }
        }
    }
    
    /// Handle link click
    private func handleLinkClick(url: String) {
        // Open URL directly in browser
        guard let urlObject = URL(string: url) else {
            return
        }
        
        // Open URL in main thread
        DispatchQueue.main.async {
            if UIApplication.shared.canOpenURL(urlObject) {
                UIApplication.shared.open(urlObject, options: [:]) { success in
                    if success {
                        // Success
                    }
                }
            }
        }
    }
}

#endif // AGENUI_SDK_BUILD