//
//  CheckBoxComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

#if AGENUI_SDK_BUILD
import UIKit

/// CheckBox component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - label: Checkbox label text (String)
/// - value: Checkbox state, supports literalBoolean or path for two-way data binding (Boolean)
/// - checks: Validation result for displaying error messages (Dictionary)
///
/// Design notes:
/// - Uses CheckBoxButton as the base control with unified checkbox style
/// - Supports two-way data binding, automatically syncs to C++ DataModel when toggled
/// - Supports validation error display with error label
class CheckBoxComponent: Component {
    
    // MARK: - Properties
    
    private var checkBoxButton: CheckBoxButton?
    private var errorLabel: UILabel?
    private var dataBindingPath: String?
    private var isUpdatingFromNative = false
    
    // MARK: - Style Configuration Properties
    
    private var checkboxSize: CGFloat = 16
    private var checkboxBorderWidth: CGFloat = 1.5
    private var checkboxBorderRadius: CGFloat = 6
    private var selectedBackgroundColor: UIColor = UIColor(red: 0x2E/255.0, green: 0x82/255.0, blue: 0xFF/255.0, alpha: 1.0)
    private var selectedBorderColor: UIColor = UIColor(red: 0x2E/255.0, green: 0x82/255.0, blue: 0xFF/255.0, alpha: 1.0)
    private var unselectedBackgroundColor: UIColor = .clear
    private var unselectedBorderColor: UIColor = UIColor.black.withAlphaComponent(0.1)
    private var disabledBackgroundColor: UIColor = UIColor(red: 0xEB/255.0, green: 0xEB/255.0, blue: 0xEB/255.0, alpha: 1.0)
    private var disabledBorderColor: UIColor = UIColor.black.withAlphaComponent(0.1)
    private var textMargin: CGFloat = 8
    private var textColor: UIColor = .black
    private var textColorDisabled: UIColor = UIColor.black.withAlphaComponent(0.4)
    private var textSize: CGFloat = 16
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "CheckBox", properties: properties)
        
        // Configure self (Component itself is a UIView)
        backgroundColor = .clear
        
        // Load local style configuration
        loadLocalStyleConfig()
        
            // Create CheckBoxButton
        createCheckBoxButton()
        
        // Create error label
        createErrorLabel()
        
        // Apply initial properties
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Measurement Override
    
    /// Measure the intrinsic size of the CheckBox component
    ///
    /// Logic:
    /// 1. Parse label text from paramJson
    /// 2. Read checkboxSize, textMargin, textSize from local style config
    /// 3. Measure text height using NSAttributedString.boundingRect
    /// 4. Height = max(checkboxSize, textHeight)
    /// 5. Apply MeasureMode constraints
    override class func measure(type: String, paramJson: String, maxWidth: Float, widthMode: MeasureMode, maxHeight: Float, heightMode: MeasureMode) -> CGSize {
        // 1. Parse paramJson
        guard let jsonData = paramJson.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            return .zero
        }
        
        // 2. Extract label text
        let labelText = extractLabelText(from: json)
        
        // 3. Load style config values
        var checkboxSize: CGFloat = 16
        var textMargin: CGFloat = 8
        var textSize: CGFloat = 16
        
        if let config = ComponentStyleConfigManager.shared.getConfig(for: "CheckBox") {
            if let size = config["checkbox-size"] as? String,
               let value = ComponentStyleConfigManager.parseSize(size) {
                checkboxSize = value
            }
            if let margin = config["text-margin"] as? String,
               let value = ComponentStyleConfigManager.parseSize(margin) {
                textMargin = value
            }
            if let size = config["text-size"] as? String,
               let value = ComponentStyleConfigManager.parseSize(size) {
                textSize = value
            }
        }
        
        // 4. Measure text height
        let constraintWidth: CGFloat = (widthMode == .undefined) ? .greatestFiniteMagnitude : CGFloat(maxWidth)
        let textAvailWidth = max(1.0, constraintWidth - checkboxSize - textMargin)
        
        var measuredHeight: CGFloat = checkboxSize
        if !labelText.isEmpty {
            let font = UIFont.systemFont(ofSize: textSize, weight: .regular)
            let attributedString = NSAttributedString(string: labelText, attributes: [.font: font])
            let textBounds = attributedString.boundingRect(
                with: CGSize(width: textAvailWidth, height: .greatestFiniteMagnitude),
                options: [.usesLineFragmentOrigin, .usesFontLeading],
                context: nil)
            measuredHeight = max(checkboxSize, ceil(textBounds.size.height))
        }
        
        var measuredWidth: CGFloat = constraintWidth
        
        // 5. Apply MeasureMode constraints
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
    
    /// Extract label text from measure JSON
    private class func extractLabelText(from json: [String: Any]) -> String {
        if let label = json["label"] as? String {
            return label
        } else {
            return ""
        }
    }
    
    // MARK: - Component Override
    
    override func layoutSubviews() {
        super.layoutSubviews()
        
        let boundsWidth = bounds.width
        var currentY: CGFloat = 0
        
        // Layout checkBoxButton at the top
        if let button = checkBoxButton {
            let buttonSize = button.sizeThatFits(CGSize(width: boundsWidth, height: .greatestFiniteMagnitude))
            button.frame = CGRect(x: 0, y: currentY, width: boundsWidth, height: buttonSize.height)
            currentY += buttonSize.height
        }
        
        // Layout errorLabel below checkBoxButton if visible
        if let label = errorLabel, !label.isHidden {
            let labelSize = label.sizeThatFits(CGSize(width: boundsWidth, height: .greatestFiniteMagnitude))
            label.frame = CGRect(x: 0, y: currentY, width: boundsWidth, height: labelSize.height)
        }
    }
    
    override func updateProperties(_ properties: [String: Any]) {
        // Call parent method to apply CSS properties to self
        super.updateProperties(properties)
        
        // Update label text
        if let label = properties["label"] {
            let labelText = extractTextValue(label)
            checkBoxButton?.label = labelText
        }
        
        // Update checkbox state (from C++ data update)
        if let value = properties["value"] {
            // Extract data binding path
            if let valueDict = value as? [String: Any], let path = valueDict["path"] as? String {
                dataBindingPath = path
            }
            
            // Update checkbox state
            isUpdatingFromNative = true
            let checked = CSSPropertyParser.extractBooleanValue(value)
            
            if let checkBoxButton = checkBoxButton, checkBoxButton.isSelected != checked {
                checkBoxButton.isSelected = checked
            }
            isUpdatingFromNative = false
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
            
            // Control editability and visual feedback
            checkBoxButton?.isEnabled = result
            let alpha: CGFloat = result ? 1.0 : 0.5
            checkBoxButton?.alpha = alpha
        }
    }
    
    // MARK: - Configuration Methods
    
    /// Load local style configuration
    private func loadLocalStyleConfig() {
        guard let config = ComponentStyleConfigManager.shared.getConfig(for: componentType) else {
            Logger.shared.debug("Using default configuration")
            return
        }
        
        Logger.shared.info("Loading local style configuration")
        
        // Parse checkbox size
        if let size = config["checkbox-size"] as? String,
           let value = ComponentStyleConfigManager.parseSize(size) {
            self.checkboxSize = value
        }
        
        // Parse border width
        if let width = config["checkbox-border-width"] as? String,
           let value = ComponentStyleConfigManager.parseSize(width) {
            self.checkboxBorderWidth = value
        }
        
        // Parse border radius
        if let radius = config["checkbox-border-radius"] as? String,
           let value = ComponentStyleConfigManager.parseSize(radius) {
            self.checkboxBorderRadius = value
        }
        
        // Parse selected state colors
        if let color = config["checkbox-background-color-selected"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.selectedBackgroundColor = value
        }
        
        if let color = config["checkbox-border-color-selected"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.selectedBorderColor = value
        }
        
        // Parse unselected state colors
        if let color = config["checkbox-background-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.unselectedBackgroundColor = value
        }
        
        if let color = config["checkbox-border-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.unselectedBorderColor = value
        }
        
        // Parse disabled state colors
        if let color = config["checkbox-background-color-disabled"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.disabledBackgroundColor = value
        }
        
        if let color = config["checkbox-border-color-disabled"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.disabledBorderColor = value
        }
        
        // Parse text styles
        if let margin = config["text-margin"] as? String,
           let value = ComponentStyleConfigManager.parseSize(margin) {
            self.textMargin = value
        }
        
        if let color = config["text-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.textColor = value
        }
        
        if let color = config["text-color-disabled"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.textColorDisabled = value
        }
        
        if let size = config["text-size"] as? String,
           let value = ComponentStyleConfigManager.parseSize(size) {
            self.textSize = value
        }
    }
    
    // MARK: - Private Methods - UI Creation
    
    /// Create CheckBoxButton
    private func createCheckBoxButton() {
        let button = CheckBoxButton()
        button.addTarget(self, action: #selector(checkBoxButtonTapped(_:)), for: .touchUpInside)
        
        // Apply configuration to CheckBoxButton
        button.checkboxSize = checkboxSize
        button.checkboxBorderWidth = checkboxBorderWidth
        button.checkboxBorderRadius = checkboxBorderRadius
        button.selectedBackgroundColor = selectedBackgroundColor
        button.selectedBorderColor = selectedBorderColor
        button.unselectedBackgroundColor = unselectedBackgroundColor
        button.unselectedBorderColor = unselectedBorderColor
        button.disabledBackgroundColor = disabledBackgroundColor
        button.disabledBorderColor = disabledBorderColor
        button.textMargin = textMargin
        button.textColor = textColor
        button.textColorDisabled = textColorDisabled
        button.textSize = textSize
        
        self.checkBoxButton = button
        addSubview(button)
    }
    
    /// Create error label
    private func createErrorLabel() {
        let label = UILabel()
        label.font = UIFont.systemFont(ofSize: 12)
        label.textColor = .red
        label.numberOfLines = 0
        label.isHidden = true
        
        self.errorLabel = label
        addSubview(label)
    }
    
    // MARK: - Private Methods - Value Extraction
    
    /// Extract text value (supports literalString or path)
    private func extractTextValue(_ value: Any) -> String {
        if let valueDict = value as? [String: Any] {
            // Support literalString
            if let literalString = valueDict["literalString"] as? String {
                return literalString
            }
            
            // Support path (data binding)
            if valueDict["path"] != nil {
                // Logic to get value from DataModel is handled by C++
                // Return empty string here, waiting for C++ update
                return ""
            }
        }
        
        // Direct string
        return String(describing: value)
    }
    
    // MARK: - Private Methods - Error Display
    
    /// Show error message
    private func showError(_ message: String) {
        errorLabel?.text = message
        errorLabel?.isHidden = false
        setNeedsLayout()
    }
    
    /// Hide error message
    private func hideError() {
        errorLabel?.text = nil
        errorLabel?.isHidden = true
        setNeedsLayout()
    }
    
    // MARK: - Private Methods - Data Binding
    
    /// Send data change to C++ DataBinding Module
    private func sendDataChangeToNative(_ value: Bool) {
        Logger.shared.debug("Syncing data to native: value=\(value)")
        syncState(["checked": value])
    }
    
    // MARK: - Event Handlers
    
    /// CheckBoxButton tap handler
    @objc private func checkBoxButtonTapped(_ sender: CheckBoxButton) {
        guard !isUpdatingFromNative else { return }
        
        // Toggle selected state
        sender.isSelected = !sender.isSelected
        
        // Send data change
        syncState(["checked": sender.isSelected])
    }
}

#endif // AGENUI_SDK_BUILD