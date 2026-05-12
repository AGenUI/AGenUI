//
//  ChoicePickerComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// ChoicePicker component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - variant: Selection mode - "mutuallyExclusive" for single selection, "multipleSelection" for multi selection (String, default "mutuallyExclusive")
/// - options: Option list, each with label (String) and value (String) (Array)
/// - value: Currently selected value - String for single, [String] for multi (String/Array)
/// - checks: Validation result for displaying error messages (Dictionary)
/// - styles: Style configuration with orientation - "vertical" (default) or "horizontal" (Dictionary)
///
/// Design notes:
/// - Uses CheckBoxButton for both single and multi selection modes
/// - Single selection acts as radio buttons (only one can be selected)
/// - Supports vertical and horizontal layout orientations
class ChoicePickerComponent: Component {
    
    // MARK: - Properties
    
    private var optionsContainer: UIView?
    private var errorLabel: UILabel?
    private var isUpdatingFromNative = false
    
    private var variant: String = "mutuallyExclusive" // Default single selection
    private var options: [[String: Any]] = []
    private var orientation: String = "vertical" // Default vertical layout
    
    // Option buttons (both single and multi selection use CheckBoxButton)
    private var optionButtons: [CheckBoxButton] = []
    private var selectedRadioIndex: Int?
    
    // MARK: - Style Configuration Properties
    
    private var checkboxSize: CGFloat = 16
    private var checkboxBorderWidth: CGFloat = 1.5
    private var checkboxBorderRadius: CGFloat = 6
    private var selectedBackgroundColor: UIColor = UIColor(red: 0x2E/255.0, green: 0x82/255.0, blue: 0xFF/255.0, alpha: 1.0)
    private var selectedBorderColor: UIColor = UIColor(red: 0x2E/255.0, green: 0x82/255.0, blue: 0xFF/255.0, alpha: 1.0)
    private var unselectedBackgroundColor: UIColor = .clear
    private var unselectedBorderColor: UIColor = UIColor.black.withAlphaComponent(0.1)
    private var textMargin: CGFloat = 8
    private var textColor: UIColor = .black
    private var textSize: CGFloat = 16
    private var choiceGap: CGFloat = 4  // Gap between options
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "ChoicePicker", properties: properties)
        
        // Load style configuration
        loadLocalStyleConfig()
        
        // Create options container
        let optionsView = UIView()
        self.optionsContainer = optionsView
        addSubview(optionsView)
        
        // Create error label
        createErrorLabel()
        
        // Apply initial properties
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Measurement Override
    
    /// Measure the intrinsic size of the ChoicePicker component
    ///
    /// Logic (aligned with Harmony choice_picker_component_measurement.cpp):
    /// 1. Parse options array and orientation from paramJson
    /// 2. Read checkboxSize, textMargin, textSize, choiceGap, etc. from local style config
    /// 3. Measure text height for each item, compute contentH = max(checkboxH, textHeight)
    /// 4. Vertical layout: accumulate itemH + choiceGap; Horizontal layout: take max itemH
    /// 5. Apply MeasureMode constraints
    override class func measure(paramJson: String, maxWidth: Float, widthMode: MeasureMode, maxHeight: Float, heightMode: MeasureMode) -> CGSize {
        // 1. Parse paramJson
        guard let jsonData = paramJson.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            return .zero
        }
        
        // 2. Extract options and orientation
        var optionLabels: [String] = []
        var horizontal = false
        
        if let optionsArray = json["options"] as? [[String: Any]] {
            for item in optionsArray {
                if let label = item["label"] as? String {
                    optionLabels.append(label)
                }
            }
        }
        
        if let styles = json["styles"] as? [String: Any],
           let ori = styles["orientation"] as? String {
            horizontal = (ori == "horizontal")
        }
        
        if optionLabels.isEmpty {
            return .zero
        }
        
        // 3. Load style config values
        var checkboxSize: CGFloat = 16
        var textMargin: CGFloat = 8
        var textSize: CGFloat = 16
        var choiceGap: CGFloat = 4
        
        if let config = ComponentStyleConfigManager.shared.getConfig(for: "ChoicePicker"),
           let pickerConfig = config["ChoicePicker"] as? [String: Any] {
            if let size = pickerConfig["checkbox-size"] as? String,
               let value = ComponentStyleConfigManager.parseSize(size) {
                checkboxSize = value
            }
            if let margin = pickerConfig["text-margin"] as? String,
               let value = ComponentStyleConfigManager.parseSize(margin) {
                textMargin = value
            }
            if let size = pickerConfig["text-size"] as? String,
               let value = ComponentStyleConfigManager.parseSize(size) {
                textSize = value
            }
            if let gap = pickerConfig["choice-gap"] as? String,
               let value = ComponentStyleConfigManager.parseSize(gap) {
                choiceGap = value
            }
        }
        
        // 4. Measure each option
        let constraintWidth: CGFloat = (widthMode == .undefined) ? .greatestFiniteMagnitude : CGFloat(maxWidth)
        let checkboxH = checkboxSize  // iOS: no extra margin like Harmony's checkboxMar
        let font = UIFont.systemFont(ofSize: textSize, weight: .regular)
        
        var totalHeight: CGFloat = 0
        var firstItem = true
        
        for text in optionLabels {
            var contentH = checkboxH
            if !text.isEmpty {
                let attributedString = NSAttributedString(string: text, attributes: [.font: font])
                let textAvailWidth = max(1.0, constraintWidth - checkboxSize - textMargin)
                let textBounds = attributedString.boundingRect(
                    with: CGSize(width: textAvailWidth, height: .greatestFiniteMagnitude),
                    options: [.usesLineFragmentOrigin, .usesFontLeading],
                    context: nil)
                contentH = max(checkboxH, ceil(textBounds.size.height))
            }
            
            if horizontal {
                totalHeight = max(totalHeight, contentH)
            } else {
                if !firstItem { totalHeight += choiceGap }
                totalHeight += contentH
                firstItem = false
            }
        }
        
        var measuredWidth: CGFloat = constraintWidth
        var measuredHeight: CGFloat = totalHeight
        
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
    
    // MARK: - Component Override
    
    override func layoutSubviews() {
        super.layoutSubviews()
        
        let boundsWidth = bounds.width
        var currentY: CGFloat = 0
        
        if orientation == "horizontal" {
            // Horizontal layout: buttons side-by-side
            var currentX: CGFloat = 0
            var maxItemHeight: CGFloat = 0
            
            for button in optionButtons {
                let buttonSize = button.sizeThatFits(CGSize(width: boundsWidth, height: .greatestFiniteMagnitude))
                let itemWidth = buttonSize.width
                let itemHeight = buttonSize.height
                button.frame = CGRect(x: currentX, y: 0, width: itemWidth, height: itemHeight)
                currentX += itemWidth + choiceGap
                maxItemHeight = max(maxItemHeight, itemHeight)
            }
            currentY = maxItemHeight
        } else {
            // Vertical layout: buttons stacked
            for button in optionButtons {
                let buttonSize = button.sizeThatFits(CGSize(width: boundsWidth, height: .greatestFiniteMagnitude))
                button.frame = CGRect(x: 0, y: currentY, width: boundsWidth, height: buttonSize.height)
                currentY += buttonSize.height + choiceGap
            }
            // Remove last gap
            if !optionButtons.isEmpty {
                currentY -= choiceGap
            }
        }
        
        // Layout errorLabel below options if visible
        if let label = errorLabel, !label.isHidden {
            let labelSize = label.sizeThatFits(CGSize(width: boundsWidth, height: .greatestFiniteMagnitude))
            label.frame = CGRect(x: 0, y: currentY, width: boundsWidth, height: labelSize.height)
        }
    }
    
    override func updateProperties(_ properties: [String: Any]) {
        super.updateProperties(properties)
        
        // Update variant
        if let variantValue = properties["variant"] as? String {
            variant = variantValue
        }
        
        // Update options
        if let optionsValue = properties["options"] as? [[String: Any]] {
            options = optionsValue
        }
        
        // Update orientation (from styles.base.orientation)
        if let styles = properties["styles"] as? [String: Any],
           let orientationValue = styles["orientation"] as? String {
            orientation = orientationValue
        }
        
        // Recreate options view
        recreateOptions()
        
        // Update selected state (data update from C++)
        if let value = properties["value"] {
            isUpdatingFromNative = true
            updateSelectedValue(value)
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
            let alpha: CGFloat = result ? 1.0 : 0.5
            let isEnabled = result
            
            optionButtons.forEach { button in
                button.isEnabled = isEnabled
                button.alpha = alpha
            }
        }
    }
    
    // MARK: - Configuration Methods
    
    /// Load local style configuration
    private func loadLocalStyleConfig() {
        guard let config = ComponentStyleConfigManager.shared.getConfig(for: componentType) else {
            return
        }

        guard let pickerConfig = config["ChoicePicker"] as? [String: Any] else {
            return
        }
        
        // Parse checkbox size
        if let size = pickerConfig["checkbox-size"] as? String,
           let value = ComponentStyleConfigManager.parseSize(size) {
            self.checkboxSize = value
        }
        
        // Parse border width
        if let width = pickerConfig["checkbox-border-width"] as? String,
           let value = ComponentStyleConfigManager.parseSize(width) {
            self.checkboxBorderWidth = value
        }
        
        // Parse border radius
        if let radius = pickerConfig["checkbox-border-radius"] as? String,
           let value = ComponentStyleConfigManager.parseSize(radius) {
            self.checkboxBorderRadius = value
        }
        
        // Parse selected state colors
        if let color = pickerConfig["checkbox-background-color-selected"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.selectedBackgroundColor = value
        }
        
        if let color = pickerConfig["checkbox-border-color-selected"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.selectedBorderColor = value
        }
        
        // Parse unselected state colors
        if let color = pickerConfig["checkbox-background-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.unselectedBackgroundColor = value
        }
        
        if let color = pickerConfig["checkbox-border-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.unselectedBorderColor = value
        }
        
        // Parse text styles
        if let margin = pickerConfig["text-margin"] as? String,
           let value = ComponentStyleConfigManager.parseSize(margin) {
            self.textMargin = value
        }
        
        if let color = pickerConfig["text-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            self.textColor = value
        }
        
        if let size = pickerConfig["text-size"] as? String,
           let value = ComponentStyleConfigManager.parseSize(size) {
            self.textSize = value
        }
        
        // Parse option gap
        if let gap = pickerConfig["choice-gap"] as? String,
           let value = ComponentStyleConfigManager.parseSize(gap) {
            self.choiceGap = value
        }
    }
    
    // MARK: - Private Methods - UI Creation
    
    /// Recreate options view
    private func recreateOptions() {
        // Clear existing views
        optionButtons.removeAll()
        selectedRadioIndex = nil
        
        guard let optionsContainer = optionsContainer else { return }
        
        // Remove all subviews
        optionsContainer.subviews.forEach { $0.removeFromSuperview() }
        
        // Create options (both single and multi selection use CheckBoxButton)
        createOptions(in: optionsContainer)
    }
    
    /// Create options (single and multi selection both use CheckBoxButton)
    private func createOptions(in container: UIView) {
        for (index, option) in options.enumerated() {
            let label = extractTextValue(option["label"])
            let value = option["value"] as? String ?? ""

            let button = CheckBoxButton()
            button.label = label
            button.value = value
            button.tag = index

            // Apply configuration to CheckBoxButton
            button.checkboxSize = checkboxSize
            button.checkboxBorderWidth = checkboxBorderWidth
            button.checkboxBorderRadius = checkboxBorderRadius
            button.selectedBackgroundColor = selectedBackgroundColor
            button.selectedBorderColor = selectedBorderColor
            button.unselectedBackgroundColor = unselectedBackgroundColor
            button.unselectedBorderColor = unselectedBorderColor
            button.textMargin = textMargin
            button.textColor = textColor
            button.textSize = textSize

            if variant == "mutuallyExclusive" {
                button.addTarget(self, action: #selector(radioButtonTapped(_:)), for: .touchUpInside)
            } else {
                button.addTarget(self, action: #selector(checkBoxButtonTapped(_:)), for: .touchUpInside)
            }

            optionButtons.append(button)
            container.addSubview(button)
        }
        
        setNeedsLayout()
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
    
    // MARK: - Private Methods - Value Update
    
    /// Update selected value
    private func updateSelectedValue(_ value: Any) {
        if variant == "mutuallyExclusive" {
            // Single selection mode
            updateRadioSelection(value as? String)
        } else {
            // Multi selection mode
            updateCheckBoxSelection(value as? [String] ?? [])
        }
    }
    
    /// Update radio button selected state
    private func updateRadioSelection(_ selectedValue: String?) {
        for (index, button) in optionButtons.enumerated() {
            let isSelected = button.value == selectedValue
            button.isSelected = isSelected
            if isSelected {
                selectedRadioIndex = index
            }
        }
    }
    
    /// Update checkbox selected state
    private func updateCheckBoxSelection(_ selectedValues: [String]) {
        for button in optionButtons {
            button.isSelected = selectedValues.contains(button.value)
        }
    }
    
    // MARK: - Private Methods - Value Extraction
    
    /// Extract text value
    private func extractTextValue(_ value: Any?) -> String {
        guard let value = value else { return "" }
        
        if let valueDict = value as? [String: Any] {
            if let literalString = valueDict["literalString"] as? String {
                return literalString
            }
            if valueDict["path"] != nil {
                return ""
            }
        }
        
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
    
    // MARK: - Event Handlers
    
    /// Radio button tap handler
    @objc private func radioButtonTapped(_ sender: CheckBoxButton) {
        guard !isUpdatingFromNative else { return }
        
        let index = sender.tag
        
        // Update selected state (single selection mode: only one can be selected)
        for (i, button) in optionButtons.enumerated() {
            button.isSelected = (i == index)
        }
        
        selectedRadioIndex = index
        
        // Send data change
        syncState(["value": sender.value])
    }
    
    /// Checkbox button tap handler
    @objc private func checkBoxButtonTapped(_ sender: CheckBoxButton) {
        guard !isUpdatingFromNative else { return }
        
        // Toggle selected state (multi selection mode: multiple can be selected)
        sender.isSelected = !sender.isSelected
        
        // Collect all selected values
        var selectedValues: [String] = []
        for button in optionButtons {
            if button.isSelected {
                selectedValues.append(button.value)
            }
        }
        
        syncState(["value": selectedValues])
    }
}
