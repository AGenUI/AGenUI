//
//  TextFieldComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

#if AGENUI_SDK_BUILD
import UIKit

/// TextFieldComponent component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - label: Label text (displayed as placeholder)
/// - value: Text value (supports literalString or path for data binding)
/// - variant: Input type (longText, number, shortText, obscured)
/// - checks: Validation result (for displaying error messages)
///
/// Style configuration (from localConfig.json):
/// - font-family: Font family name (String, default "PingFang SC")
/// - font-size: Font size (String, default 16)
/// - line-height: Line height (String, default 0 uses system)
/// - letter-spacing: Letter spacing (String, default 0)
/// - color: Text color (String, default black)
/// - placeholder.color: Placeholder color (String)
/// - placeholder.font-size: Placeholder font size (String)
///
/// Design notes:
/// - Uses UITextField for single-line input, UITextView for multi-line (longText variant)
/// - Supports two-way data binding, auto-syncs to C++ DataModel on user input
/// - Supports validation error display with red border and error label
/// - Dynamic input control switching when variant changes at runtime
class TextFieldComponent: Component {
    
    // MARK: - Properties
    
    private var textField: UITextField?
    private var textView: UITextView?
    private var errorLabel: UILabel?
    private var dataBindingPath: String?
    private var isUpdatingFromNative = false
    private var currentVariant: String = "shortText"

    // Validation regexp support
    private var validationRegexp: String?
    private var validationError: String?
    private var isValid: Bool = true

    // Style configuration properties
    private var fontFamily: String = "PingFang SC"
    private var fontSize: CGFloat = 16
    private var lineHeight: CGFloat = 0  // 0 means use default value
    private var letterSpacing: CGFloat = 0
    private var textColor: UIColor = .black
    
    // Placeholder style configuration
    private var placeholderText: String = ""
    private var placeholderColor: UIColor = UIColor(red: 0.6, green: 0.6, blue: 0.6, alpha: 1.0)
    private var placeholderFont: UIFont?
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        // Component itself is a UIView
        super.init(componentId: componentId, componentType: "TextField", properties: properties)
        
        // Load local style configuration (before creating UI)
        loadLocalStyleConfig()
        
        // Decide which input control to create based on initial properties
        let variant = properties["variant"] as? String ?? "shortText"
        currentVariant = variant
        
        if variant.lowercased() == "longtext" {
            createTextView()
        } else {
            createTextField()
        }
        
        // Create error label
        createErrorLabel()
        
        // Apply initial properties after view is created
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Measurement Override

    override class func measure(type: String,
                                paramJson: String,
                                maxWidth: Float,
                                widthMode: MeasureMode,
                                maxHeight: Float,
                                heightMode: MeasureMode) -> CGSize {
        // 1. Parse paramJson
        guard let jsonData = paramJson.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            return CGSize(width: CGFloat(maxWidth), height: 44)
        }

        // 2. Resolve font size from localConfig or default
        var fontSize: CGFloat = 16
        if let config = ComponentStyleConfigManager.shared.getConfig(for: "TextField") {
            if let sizeStr = config["font-size"] as? String {
                let parsed = CSSPropertyParser.parseOffset(sizeStr)
                if case .number(let v) = parsed { fontSize = v }
            }
        }

        // 3. Build font
        var fontFamily = "PingFang SC"
        if let config = ComponentStyleConfigManager.shared.getConfig(for: "TextField"),
           let family = config["font-family"] as? String {
            fontFamily = family
        }
        let font: UIFont = UIFont(name: fontFamily, size: fontSize) ?? UIFont.systemFont(ofSize: fontSize)

        // 4. Determine if multi-line (longText variant)
        let variant = (json["variant"] as? String ?? "shortText").lowercased()
        let isMultiLine = variant == "longtext"

        // 5. Calculate constraint width
        let constraintWidth: CGFloat = (widthMode == .undefined)
            ? .greatestFiniteMagnitude
            : CGFloat(maxWidth)

        // 6. Calculate height
        let singleLineHeight = font.lineHeight + 16  // 16pt vertical padding
        let multiLineMinHeight: CGFloat = singleLineHeight * 3

        var measuredWidth: CGFloat
        var measuredHeight: CGFloat

        if widthMode == .exactly {
            measuredWidth = CGFloat(maxWidth)
        } else {
            measuredWidth = constraintWidth
        }

        if heightMode == .exactly {
            measuredHeight = CGFloat(maxHeight)
        } else if isMultiLine {
            measuredHeight = multiLineMinHeight
        } else {
            measuredHeight = singleLineHeight
        }

        return CGSize(width: measuredWidth, height: measuredHeight)
    }

    // MARK: - Layout

    override func layoutSubviews() {
        super.layoutSubviews()

        if let errorLabel = errorLabel, !errorLabel.isHidden {
            // errorLabel displayed below the input field, estimated height 20pt
            let errorLabelHeight: CGFloat = 20
            let inputHeight = bounds.height - errorLabelHeight
            textField?.frame = CGRect(x: 0, y: 0, width: bounds.width, height: inputHeight)
            textView?.frame = CGRect(x: 0, y: 0, width: bounds.width, height: inputHeight)
            errorLabel.frame = CGRect(x: 0, y: inputHeight, width: bounds.width, height: errorLabelHeight)
        } else {
            textField?.frame = bounds
            textView?.frame = bounds
            errorLabel?.frame = CGRect(x: 0, y: bounds.height, width: bounds.width, height: 20)
        }

        // Vertically center the placeholder in UITextView when no real text is entered.
        // UITextView is multi-line by nature, so we only do this while showing the placeholder;
        // once the user starts typing, the standard top-aligned behavior is restored elsewhere.
        if let tv = textView {
            let lineHeight = tv.font?.lineHeight ?? 17
            let topInset = max(0, (tv.bounds.height - lineHeight) / 2)
            tv.textContainerInset = UIEdgeInsets(top: topInset, left: 5, bottom: 0, right: 5)
        }
    }

    override func updateProperties(_ properties: [String: Any]) {
        super.updateProperties(properties)
        
        // Check if input control type needs switching
        if let variant = properties["variant"] as? String {
            if variant != currentVariant {
                switchInputControl(to: variant)
            }
        }
        
        // 1. Update placeholder first
        if let label = properties["label"] {
            let labelText = CSSPropertyParser.extractStringValue(label)
            updatePlaceholder(labelText)
        }
        
        // Support independent placeholder property
//        if let placeholder = properties["placeholder"] {
//            let placeholderText = CSSPropertyParser.extractStringValue(placeholder)
//            updatePlaceholder(placeholderText)
//        }
        
        // 2. Then update text value (data update from C++)
        if let value = properties["value"] {
            // Extract data binding path
            if let valueDict = value as? [String: Any], let path = valueDict["path"] as? String {
                dataBindingPath = path
            }
            
            // Update text content
            isUpdatingFromNative = true
            let text = CSSPropertyParser.extractStringValue(value)
            updateTextValue(text)
            isUpdatingFromNative = false
        }
        
        // Update input type
        if let variant = properties["variant"] as? String {
            applyVariant(variant)
        }

        // Update validation regexp
        if let regexp = properties["validationRegexp"] as? String {
            validationRegexp = regexp
            // Validate current value when regexp changes
            validateCurrentInput()
        }

        // checks adaptation - display validation errors
        if let checks = properties["checks"] as? [String: Any] {
            let result = checks["result"] as? Bool ?? true
            let message = checks["message"] as? String ?? ""
            
            if !result && !message.isEmpty {
                showError(message)
            } else {
                hideError()
            }
            
            // Control editability
            textField?.isEnabled = result
            textView?.isEditable = result
            
            // Visual feedback
            let alpha: CGFloat = result ? 1.0 : 0.5
            textField?.alpha = alpha
            textView?.alpha = alpha
        }
    }
    
    // MARK: - Private Methods - UI Creation
    
    /// Create single-line text input
    private func createTextField() {
        let field = PaddedTextField()
        // Apply style configuration
        applyTextStyle(to: field)
        
        // Apply placeholder style
        applyPlaceholderStyle(to: field)
        
        // Add text change observer
        field.addTarget(self, action: #selector(textFieldDidChange(_:)), for: .editingChanged)
        
        self.textField = field
        
        // Add to Component
        addSubview(field)
    }
    
    /// Create multi-line text input
    private func createTextView() {
        let view = UITextView()
        // Make background transparent so the component's background color shows through
        // (UITextView defaults to white, unlike UITextField which is transparent)
        view.backgroundColor = .clear
        //view.textContainerInset = UIEdgeInsets(top: 8, left: 12, bottom: 8, right: 12)
        // Apply style configuration
        applyTextStyle(to: view)
        
        // Initialize to placeholder state
        view.text = placeholderText
        view.textColor = placeholderColor
        
        // Use notification to observe text changes instead of delegate
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(textViewDidChange(_:)),
            name: UITextView.textDidChangeNotification,
            object: view
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(textViewDidBeginEditing(_:)),
            name: UITextView.textDidBeginEditingNotification,
            object: view
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(textViewDidEndEditing(_:)),
            name: UITextView.textDidEndEditingNotification,
            object: view
        )
        
        self.textView = view
        
        // Add to Component
        addSubview(view)
    }
    
    /// Create error label
    private func createErrorLabel() {
        let label = UILabel()
        label.font = UIFont.systemFont(ofSize: 12)
        label.textColor = .red
        label.numberOfLines = 0
        label.isHidden = true
        
        self.errorLabel = label
        
        // Add to Component
        addSubview(label)
    }
    
    // MARK: - Private Methods - Input Control Switching
    
    /// Switch input control type
    private func switchInputControl(to variant: String) {
        // Save current text
        let currentText = textField?.text ?? textView?.text ?? ""
        
        // Remove old control
        textField?.removeFromSuperview()
        textView?.removeFromSuperview()
        errorLabel?.removeFromSuperview()
        textField = nil
        textView = nil
        errorLabel = nil
        
        // Update current variant
        currentVariant = variant
        
        // Create new control
        if variant.lowercased() == "longtext" {
            createTextView()
            textView?.text = currentText
        } else {
            createTextField()
            textField?.text = currentText
        }
        
        // Recreate error label
        createErrorLabel()
        
        // Re-apply styles and variant
        if let field = textField {
            applyTextStyle(to: field)
        } else if let view = textView {
            applyTextStyle(to: view)
        }
        applyVariant(variant)
    }
    
    // MARK: - Configuration Methods
    
    /// Load local style configuration
    private func loadLocalStyleConfig() {
        guard let config = ComponentStyleConfigManager.shared.getConfig(for: componentType) else {
            Logger.shared.debug("Using default configuration")
            return
        }
        
        Logger.shared.info("Loading local style configuration: \(config)")
        
        // Parse font configuration
        if let family = config["font-family"] as? String {
            self.fontFamily = family
        }
        
        // Use CSSPropertyParser to parse font size
        if let size = config["font-size"] as? String {
            let sizeValue = CSSPropertyParser.parseOffset(size)
            if case .number(let value) = sizeValue {
                self.fontSize = value
            }
        }
        
        // Use CSSPropertyParser to parse line height
        if let height = config["line-height"] as? String {
            // line-height may be "normal" or a numeric value
            if height.lowercased() != "normal" {
                let heightValue = CSSPropertyParser.parseOffset(height)
                if case .number(let value) = heightValue {
                    self.lineHeight = value
                }
            }
        }
        
        // Use CSSPropertyParser to parse letter spacing
        if let spacing = config["letter-spacing"] as? String {
            // letter-spacing may be "normal" or a numeric value
            if spacing.lowercased() != "normal" {
                let spacingValue = CSSPropertyParser.parseOffset(spacing)
                if case .number(let value) = spacingValue {
                    self.letterSpacing = value
                }
            }
        }
        
        // Use CSSPropertyParser to parse color
        if let color = config["color"] as? String {
            let colorValue = CSSPropertyParser.parseColor(color)
            if case .color(let value) = colorValue {
                self.textColor = value
            }
        }
        
        // Parse placeholder configuration
        if let placeholderConfig = config["placeholder"] as? [String: Any] {
            // Use CSSPropertyParser to parse placeholder color
            if let color = placeholderConfig["color"] as? String {
                let colorValue = CSSPropertyParser.parseColor(color)
                if case .color(let value) = colorValue {
                    self.placeholderColor = value
                }
            }
            
            // Use CSSPropertyParser to parse placeholder font size
            if let size = placeholderConfig["font-size"] as? String {
                let sizeValue = CSSPropertyParser.parseOffset(size)
                if case .number(let value) = sizeValue {
                    // Create placeholder-specific font
                    if let customFont = UIFont(name: fontFamily, size: value) {
                        self.placeholderFont = customFont
                    } else {
                        self.placeholderFont = UIFont.systemFont(ofSize: value)
                    }
                }
            }
        }
    }
    
    /// Apply text style to UITextField
    private func applyTextStyle(to textField: UITextField) {
        // Create font
        let font: UIFont
        if let customFont = UIFont(name: fontFamily, size: fontSize) {
            font = customFont
        } else {
            font = UIFont.systemFont(ofSize: fontSize)
        }
        textField.font = font
        textField.textColor = textColor
        
        // Apply letter spacing (if not 0)
        if letterSpacing != 0 {
            // Letter spacing needs to be set via NSAttributedString
            // Set default value here, will be applied when actual text is set
            textField.defaultTextAttributes[.kern] = letterSpacing
        }
    }
    
    /// Apply placeholder style to UITextField
    private func applyPlaceholderStyle(to textField: UITextField) {
        guard !placeholderText.isEmpty else { return }
        
        // Use NSAttributedString to set styled placeholder
        let font = placeholderFont ?? textField.font ?? UIFont.systemFont(ofSize: fontSize)
        
        var attributes: [NSAttributedString.Key: Any] = [
            .foregroundColor: placeholderColor,
            .font: font
        ]
        
        // Apply letter spacing
        if letterSpacing != 0 {
            attributes[.kern] = letterSpacing
        }
        
        textField.attributedPlaceholder = NSAttributedString(
            string: placeholderText,
            attributes: attributes
        )
    }
    
    /// Apply text style to UITextView
    private func applyTextStyle(to textView: UITextView) {
        // Create font
        let font: UIFont
        if let customFont = UIFont(name: fontFamily, size: fontSize) {
            font = customFont
        } else {
            font = UIFont.systemFont(ofSize: fontSize)
        }
        textView.font = font
        textView.textColor = textColor
        
        // Apply letter spacing and line height (if not 0)
        if letterSpacing != 0 || lineHeight != 0 {
            let paragraphStyle = NSMutableParagraphStyle()
            if lineHeight != 0 {
                paragraphStyle.lineSpacing = lineHeight - font.lineHeight
                paragraphStyle.minimumLineHeight = lineHeight
                paragraphStyle.maximumLineHeight = lineHeight
            }
            
            textView.typingAttributes = [
                .font: font,
                .foregroundColor: textColor,
                .kern: letterSpacing,
                .paragraphStyle: paragraphStyle
            ]
        }
    }
    
    // MARK: - Private Methods - Variant Handling
    
    /// Apply input type variant
    /// A2UI v0.9 protocol values: longText, number, shortText, obscured
    private func applyVariant(_ variant: String) {
        switch variant.lowercased() {
        case "number":
            textField?.keyboardType = .decimalPad
            
        case "longtext":
            // Multi-line text already handled in createTextView
            break
            
        case "obscured":
            textField?.isSecureTextEntry = true
            
        case "shorttext":
            fallthrough
        default:
            textField?.keyboardType = .default
            textField?.isSecureTextEntry = false
        }
        
    }
    
    // MARK: - Private Methods - Placeholder Management
    
    /// Update placeholder text
    private func updatePlaceholder(_ text: String) {
        placeholderText = text
        
        if let textField = textField {
            // UITextField uses native placeholder
            applyPlaceholderStyle(to: textField)
        } else if let textView = textView {
            // UITextView uses simulated placeholder
            if isTextViewShowingPlaceholder() {
                textView.text = text
                textView.textColor = placeholderColor
            }
        }
    }
    
    /// Update text value
    private func updateTextValue(_ text: String) {
        if let textField = textField {
            // UITextField: show placeholder when empty string
            textField.text = text.isEmpty ? "" : text
        } else if let textView = textView {
            // UITextView: needs to handle placeholder state
            if text.isEmpty {
                // Show placeholder
                textView.text = placeholderText
                textView.textColor = placeholderColor
            } else {
                // Show actual text
                textView.text = text
                textView.textColor = textColor
            }
        }
    }
    
    /// Check if TextView is showing placeholder
    private func isTextViewShowingPlaceholder() -> Bool {
        guard let textView = textView else { return false }
        return textView.textColor == placeholderColor && textView.text == placeholderText
    }
    
    // MARK: - Private Methods - Error Display
    
    /// Show error message
    private func showError(_ message: String) {
        errorLabel?.text = message
        errorLabel?.isHidden = false
        
        // Add red border
        textField?.layer.borderColor = UIColor.red.cgColor
        textField?.layer.borderWidth = 1.0
        textView?.layer.borderColor = UIColor.red.cgColor
        textView?.layer.borderWidth = 1.0
    }
    
    /// Hide error message
    private func hideError() {
        errorLabel?.text = nil
        errorLabel?.isHidden = true
        
        // Restore default border
        textField?.layer.borderWidth = 0
        textView?.layer.borderColor = UIColor.lightGray.cgColor
        textView?.layer.borderWidth = 1.0
    }

    // MARK: - Private Methods - Validation

    /// Validate current input against validationRegexp
    private func validateCurrentInput() {
        guard let regexp = validationRegexp else { return }

        let currentValue = textField?.text ?? textView?.text ?? ""

        // Skip validation if empty (unless regexp requires non-empty)
        if currentValue.isEmpty {
            isValid = true
            validationError = nil
            syncValidationResult()
            return
        }

        // Perform regex validation
        do {
            let regex = try NSRegularExpression(pattern: regexp, options: [])
            let range = NSRange(location: 0, length: currentValue.utf16.count)

            // Check if the entire string matches the pattern
            let matches = regex.matches(in: currentValue, options: [], range: range)

            // Full match required: the match should cover the entire string
            if let firstMatch = matches.first,
               firstMatch.range.location == 0 && firstMatch.range.length == currentValue.utf16.count {
                isValid = true
                validationError = nil
            } else {
                isValid = false
                validationError = "Invalid input format."
            }
        } catch {
            Logger.shared.error("Invalid regex pattern: \(regexp), error: \(error.localizedDescription)")
            isValid = true // Don't block input on invalid regex
            validationError = nil
        }

        syncValidationResult()
    }

    /// Sync validation result to checks property
    private func syncValidationResult() {
        var checks: [String: Any] = [:]

        if isValid {
            checks["result"] = true
            checks["message"] = ""
        } else {
            checks["result"] = false
            checks["message"] = validationError ?? "Invalid input format."
        }

        // Send validation result to native
        syncState(["checks": checks])

        // Update UI based on validation result
        if !isValid {
            showError(checks["message"] as? String ?? "")
        } else {
            hideError()
        }
    }

    // MARK: - Private Methods - Data Binding
    
    // MARK: - Event Handlers
    
    /// TextField text change handler
    @objc private func textFieldDidChange(_ textField: UITextField) {
        guard !isUpdatingFromNative else { return }

        let newValue = textField.text ?? ""
        syncState(["value": newValue])

        // Trigger validation if validationRegexp is set
        if validationRegexp != nil {
            // Debounce validation with 300ms delay
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
                self?.validateCurrentInput()
            }
        }
    }
    
    /// TextView begin editing handler
    @objc private func textViewDidBeginEditing(_ notification: Notification) {
        guard let textView = notification.object as? UITextView else { return }
        
        // Clear placeholder effect
        if isTextViewShowingPlaceholder() {
            textView.text = ""
            textView.textColor = textColor
        }
    }
    
    /// TextView end editing handler
    @objc private func textViewDidEndEditing(_ notification: Notification) {
        guard let textView = notification.object as? UITextView else { return }
        
        // If empty, show placeholder
        if textView.text.isEmpty {
            textView.text = placeholderText
            textView.textColor = placeholderColor
        }
    }
    
    /// TextView text change handler
    @objc private func textViewDidChange(_ notification: Notification) {
        guard let textView = notification.object as? UITextView else { return }
        guard !isUpdatingFromNative else { return }
        
        // Ignore text in placeholder state
        if isTextViewShowingPlaceholder() {
            return
        }
        
        let newValue = textView.text ?? ""
        syncState(["value" : newValue])

        // Trigger validation if validationRegexp is set
        if validationRegexp != nil {
            // Debounce validation with 300ms delay
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
                self?.validateCurrentInput()
            }
        }
    }

    private class PaddedTextField:UITextField{
        var contentInset = UIEdgeInsets(top:0,left:12,bottom:0,right:12)
        override func textRect(forBounds bounds:CGRect)-> CGRect{
            bounds.inset(by: contentInset);
        }
        
        override func editingRect(forBounds bounds:CGRect)-> CGRect{
            bounds.inset(by: contentInset);
        }
        
        override func placeholderRect(forBounds bounds:CGRect)-> CGRect{
            bounds.inset(by: contentInset);
        }


    }
    
    deinit {
        // Remove notification observer
        NotificationCenter.default.removeObserver(self)
    }
}

#endif // AGENUI_SDK_BUILD