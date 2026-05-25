//
//  ChoicePickerComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

#if AGENUI_SDK_BUILD
import UIKit

// MARK: - ChipButton (Internal Class)

/// Chip button for ChoicePicker chips display style
/// A rounded rectangular button that shows selected/unselected state through background color
private class ChipButton: UIButton {

    // MARK: - Properties

    var value: String = ""

    // Style configuration
    var cornerRadius: CGFloat = 16
    var borderWidth: CGFloat = 1.0
    var paddingHorizontal: CGFloat = 16
    var paddingVertical: CGFloat = 8

    // Colors
    var selectedBackgroundColor: UIColor = UIColor(red: 0x2E/255.0, green: 0x82/255.0, blue: 0xFF/255.0, alpha: 1.0)
    var selectedTextColor: UIColor = .white
    var unselectedBackgroundColor: UIColor = .clear
    var unselectedBorderColor: UIColor = UIColor.black.withAlphaComponent(0.1)
    var unselectedTextColor: UIColor = .black

    // State
    private var _isSelected: Bool = false
    override var isSelected: Bool {
        get { _isSelected }
        set {
            _isSelected = newValue
            updateAppearance()
        }
    }

    // MARK: - Initialization

    override init(frame: CGRect) {
        super.init(frame: frame)
        setupUI()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Setup

    private func setupUI() {
        titleLabel?.font = UIFont.systemFont(ofSize: 14)
        titleLabel?.numberOfLines = 1
        contentEdgeInsets = UIEdgeInsets(
            top: paddingVertical,
            left: paddingHorizontal,
            bottom: paddingVertical,
            right: paddingHorizontal
        )

        layer.borderWidth = borderWidth
        updateAppearance()
    }

    // MARK: - Appearance Update

    private func updateAppearance() {
        if _isSelected {
            backgroundColor = selectedBackgroundColor
            setTitleColor(selectedTextColor, for: .normal)
            layer.borderColor = selectedBackgroundColor.cgColor
        } else {
            backgroundColor = unselectedBackgroundColor
            setTitleColor(unselectedTextColor, for: .normal)
            layer.borderColor = unselectedBorderColor.cgColor
        }

        layer.cornerRadius = cornerRadius
        clipsToBounds = true
    }

    // MARK: - Layout

    override func layoutSubviews() {
        super.layoutSubviews()
        layer.cornerRadius = cornerRadius
    }
}

// MARK: - SearchInputView (Internal Class)

/// Search input view with search icon and clear button
private class SearchInputView: UIView {

    // MARK: - Properties

    private let searchIconImageView: UIImageView
    private let textField: UITextField
    private let clearButton: UIButton

    var onTextChanged: ((String) -> Void)?

    // Style configuration
    var cornerRadius: CGFloat = 20
    var borderColor: UIColor = UIColor.black.withAlphaComponent(0.1)
    var borderWidth: CGFloat = 1.0
    var viewBackgroundColor: UIColor = UIColor.white
    var placeholderColor: UIColor = UIColor.black.withAlphaComponent(0.4)
    var textColor: UIColor = .black
    var fontSize: CGFloat = 14
    var padding: CGFloat = 8
    var iconSize: CGFloat = 16
    var iconColor: UIColor = UIColor.black.withAlphaComponent(0.4)

    // MARK: - Initialization

    override init(frame: CGRect) {
        searchIconImageView = UIImageView()
        textField = UITextField()
        clearButton = UIButton(type: .custom)

        super.init(frame: frame)
        setupUI()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Setup

    private func setupUI() {
        // Configure search icon
        searchIconImageView.image = UIImage(systemName: "magnifyingglass")
        searchIconImageView.tintColor = iconColor
        searchIconImageView.contentMode = .scaleAspectFit

        // Configure text field
        let placeholderAttrs: [NSAttributedString.Key: Any] = [
            .foregroundColor: placeholderColor,
            .font: UIFont.systemFont(ofSize: fontSize)
        ]
        textField.attributedPlaceholder = NSAttributedString(string: "Search options...", attributes: placeholderAttrs)
        textField.font = UIFont.systemFont(ofSize: fontSize)
        textField.textColor = textColor
        textField.borderStyle = .none
        textField.delegate = self
        textField.clearButtonMode = .never

        // Configure clear button
        clearButton.setImage(UIImage(systemName: "xmark.circle.fill"), for: .normal)
        clearButton.tintColor = iconColor
        clearButton.addTarget(self, action: #selector(clearTapped), for: .touchUpInside)
        clearButton.isHidden = true

        addSubview(searchIconImageView)
        addSubview(textField)
        addSubview(clearButton)

        // Apply visual styles
        layer.cornerRadius = cornerRadius
        layer.borderColor = borderColor.cgColor
        layer.borderWidth = borderWidth
        backgroundColor = viewBackgroundColor
    }

    // MARK: - Layout

    /// Manual horizontal layout: [icon] [textField (flex)] [clearButton]
    override func layoutSubviews() {
        super.layoutSubviews()

        layer.cornerRadius = cornerRadius

        let iconY = (bounds.height - iconSize) / 2
        searchIconImageView.frame = CGRect(x: padding, y: iconY, width: iconSize, height: iconSize)

        let clearVisible = !clearButton.isHidden
        let clearWidth = iconSize
        let clearX = bounds.width - padding - clearWidth
        clearButton.frame = CGRect(x: clearX, y: iconY, width: clearWidth, height: iconSize)

        let textFieldX = padding + iconSize + padding / 2
        let rightInset: CGFloat = clearVisible ? (clearWidth + padding / 2 + padding) : padding
        let textFieldWidth = max(0, bounds.width - textFieldX - rightInset)
        textField.frame = CGRect(x: textFieldX, y: 0, width: textFieldWidth, height: bounds.height)
    }

    // MARK: - Actions

    @objc private func clearTapped() {
        textField.text = ""
        clearButton.isHidden = true
        setNeedsLayout()
        onTextChanged?("")
    }

    // MARK: - Public Methods

    func setText(_ text: String) {
        textField.text = text
        clearButton.isHidden = text.isEmpty
        setNeedsLayout()
    }

    func getText() -> String {
        return textField.text ?? ""
    }
}

// MARK: - UITextFieldDelegate

extension SearchInputView: UITextFieldDelegate {
    func textField(_ textField: UITextField, shouldChangeCharactersIn range: NSRange, replacementString string: String) -> Bool {
        return true
    }

    func textFieldDidChangeSelection(_ textField: UITextField) {
        let text = textField.text ?? ""
        let wasHidden = clearButton.isHidden
        clearButton.isHidden = text.isEmpty
        if wasHidden != clearButton.isHidden {
            setNeedsLayout()
        }
        onTextChanged?(text)
    }
}

/// ChoicePicker component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - variant: Selection mode - "mutuallyExclusive" for single selection, "multipleSelection" for multi selection (String, default "mutuallyExclusive")
/// - options: Option list, each with label (String) and value (String) (Array)
/// - value: Currently selected value - String for single, [String] for multi (String/Array)
/// - checks: Validation result for displaying error messages (Dictionary)
/// - styles: Style configuration with orientation - "vertical" (default) or "horizontal" (Dictionary)
/// - displayStyle: Display style - "checkbox" (default) or "chips" (String)
/// - filterable: Whether to show a search box that filters options by label (Bool, default false)
///
/// Design notes:
/// - Uses CheckBoxButton for checkbox display style
/// - Uses ChipButton for chips display style (row-wrap layout)
/// - Single selection acts as radio buttons (only one can be selected)
/// - Supports vertical and horizontal layout orientations (checkbox style only)
class ChoicePickerComponent: Component {

    // MARK: - Properties

    private var optionsContainer: UIView?
    private var errorLabel: UILabel?
    private var isUpdatingFromNative = false

    private var variant: String = "mutuallyExclusive" // Default single selection
    private var displayStyle: String = "checkbox" // Default checkbox style
    private var filterable: Bool = false           // Default not filterable
    private var options: [[String: Any]] = []
    private var orientation: String = "vertical" // Default vertical layout

    // Search state
    private var searchText: String = ""
    private var filteredOptions: [[String: Any]] = []

    // Option buttons for checkbox display style
    private var optionButtons: [CheckBoxButton] = []
    private var selectedRadioIndex: Int?

    // Option buttons for chips display style
    private var chipButtons: [ChipButton] = []

    // Search input view (pre-created in init, hidden until filterable=true)
    private var searchInputView: SearchInputView?
    private var noResultsLabel: UILabel?

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

    // Heights used by layoutSubviews when search/no-results are visible
    private let searchInputHeight: CGFloat = 44
    private let searchInputMargin: CGFloat = 8
    private let noResultsLabelHeight: CGFloat = 24
    private let noResultsLabelMargin: CGFloat = 8
    private let chipHeight: CGFloat = 36

    // MARK: - Initialization

    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "ChoicePicker", properties: properties)

        // Load style configuration
        loadLocalStyleConfig()

        // Pre-create search input view (kept hidden until filterable=true).
        // Pre-creation avoids reordering subviews when filterable toggles at runtime.
        let searchView = SearchInputView()
        searchView.onTextChanged = { [weak self] text in
            self?.handleSearchTextChanged(text)
        }
        searchView.isHidden = true
        self.searchInputView = searchView
        addSubview(searchView)

        // Create options container
        let optionsView = UIView()
        self.optionsContainer = optionsView
        addSubview(optionsView)

        // Create no-results label (hidden by default)
        let resultsLabel = UILabel()
        resultsLabel.text = "No matching options"
        resultsLabel.font = UIFont.systemFont(ofSize: 14)
        resultsLabel.textColor = UIColor.black.withAlphaComponent(0.5)
        resultsLabel.textAlignment = .center
        resultsLabel.isHidden = true
        self.noResultsLabel = resultsLabel
        addSubview(resultsLabel)

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
    /// 6. Adds extra space for search input and no-results label when filterable=true
    override class func measure(type: String, paramJson: String, maxWidth: Float, widthMode: MeasureMode, maxHeight: Float, heightMode: MeasureMode) -> CGSize {
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

        let displayStyle = (json["displayStyle"] as? String) ?? "checkbox"
        let filterable = (json["filterable"] as? Bool) ?? false

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

        if displayStyle == "chips" {
            // Chip wrap layout: estimate height by laying chips out in rows.
            let chipHeight: CGFloat = 36
            let chipMargin: CGFloat = choiceGap / 2
            let chipFont = UIFont.systemFont(ofSize: 14)
            let chipPaddingH: CGFloat = 16

            var rowWidth: CGFloat = 0
            var rowCount: Int = 1
            for label in optionLabels {
                let textWidth = (label as NSString).size(withAttributes: [.font: chipFont]).width
                let chipWidth = ceil(textWidth) + chipPaddingH * 2 + chipMargin * 2
                if rowWidth + chipWidth > constraintWidth && rowWidth > 0 {
                    rowCount += 1
                    rowWidth = chipWidth
                } else {
                    rowWidth += chipWidth
                }
            }
            totalHeight = CGFloat(rowCount) * (chipHeight + chipMargin * 2)
        } else {
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
        }

        // Search input adds height when filterable is on (margin top + height + margin bottom)
        if filterable {
            totalHeight += 8 + 44 + 8
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

        // 1. Search input bar at the top
        if let searchView = searchInputView, !searchView.isHidden {
            currentY += searchInputMargin
            searchView.frame = CGRect(
                x: searchInputMargin,
                y: currentY,
                width: max(0, boundsWidth - searchInputMargin * 2),
                height: searchInputHeight
            )
            searchView.setNeedsLayout()
            currentY += searchInputHeight + searchInputMargin
        }

        // 2. Options container occupies the remaining space.
        // We need to know its height up-front; compute it via layoutOptions().
        let optionsContainerOriginY = currentY
        if let container = optionsContainer {
            // Initially size container to full width; layoutOptions will compute height.
            container.frame = CGRect(x: 0, y: optionsContainerOriginY, width: boundsWidth, height: 0)
            let containerHeight = layoutOptions(in: container, width: boundsWidth)
            container.frame = CGRect(x: 0, y: optionsContainerOriginY, width: boundsWidth, height: containerHeight)
            currentY = optionsContainerOriginY + containerHeight
        }

        // 3. No-results label below options when visible
        if let label = noResultsLabel, !label.isHidden {
            currentY += noResultsLabelMargin
            label.frame = CGRect(
                x: noResultsLabelMargin,
                y: currentY,
                width: max(0, boundsWidth - noResultsLabelMargin * 2),
                height: noResultsLabelHeight
            )
            currentY += noResultsLabelHeight + noResultsLabelMargin
        }

        // 4. Error label at the very bottom when visible
        if let label = errorLabel, !label.isHidden {
            let labelSize = label.sizeThatFits(CGSize(width: boundsWidth, height: .greatestFiniteMagnitude))
            label.frame = CGRect(x: 0, y: currentY, width: boundsWidth, height: labelSize.height)
        }
    }

    /// Lay out option/chip buttons inside the given container, returning the consumed height.
    private func layoutOptions(in container: UIView, width: CGFloat) -> CGFloat {
        if displayStyle == "chips" {
            return layoutChips(in: container, width: width)
        } else {
            return layoutCheckBoxes(in: container, width: width)
        }
    }

    private func layoutCheckBoxes(in container: UIView, width: CGFloat) -> CGFloat {
        var currentY: CGFloat = 0

        if orientation == "horizontal" {
            var currentX: CGFloat = 0
            var maxItemHeight: CGFloat = 0

            for button in optionButtons {
                let buttonSize = button.sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude))
                let itemWidth = buttonSize.width
                let itemHeight = buttonSize.height
                button.frame = CGRect(x: currentX, y: 0, width: itemWidth, height: itemHeight)
                currentX += itemWidth + choiceGap
                maxItemHeight = max(maxItemHeight, itemHeight)
            }
            currentY = maxItemHeight
        } else {
            for button in optionButtons {
                let buttonSize = button.sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude))
                button.frame = CGRect(x: 0, y: currentY, width: width, height: buttonSize.height)
                currentY += buttonSize.height + choiceGap
            }
            if !optionButtons.isEmpty {
                currentY -= choiceGap
            }
        }

        return currentY
    }

    /// Manual row-wrap layout for chip buttons.
    private func layoutChips(in container: UIView, width: CGFloat) -> CGFloat {
        let horizontalInset: CGFloat = 8
        let chipMargin = choiceGap / 2
        let availableWidth = max(0, width - horizontalInset * 2)

        var currentX: CGFloat = horizontalInset
        var currentY: CGFloat = chipMargin
        var rowHeight: CGFloat = chipHeight

        for chip in chipButtons {
            let fitting = chip.sizeThatFits(CGSize(width: availableWidth, height: .greatestFiniteMagnitude))
            let chipWidth = min(fitting.width, availableWidth)
            let chipTotalWidth = chipWidth + chipMargin * 2

            // Wrap when the next chip would overflow the row.
            if currentX + chipTotalWidth > horizontalInset + availableWidth && currentX > horizontalInset {
                currentX = horizontalInset
                currentY += rowHeight + chipMargin * 2
                rowHeight = chipHeight
            }

            chip.frame = CGRect(
                x: currentX + chipMargin,
                y: currentY + chipMargin,
                width: chipWidth,
                height: chipHeight
            )
            currentX += chipTotalWidth
            rowHeight = max(rowHeight, chipHeight)
        }

        if chipButtons.isEmpty {
            return 0
        }
        return currentY + rowHeight + chipMargin
    }

    override func updateProperties(_ properties: [String: Any]) {
        super.updateProperties(properties)

        // Update variant
        if let variantValue = properties["variant"] as? String {
            variant = variantValue
        }

        // Update displayStyle
        if let displayStyleValue = properties["displayStyle"] as? String {
            displayStyle = displayStyleValue
        }

        // Update filterable
        if let filterableValue = properties["filterable"] as? Bool {
            filterable = filterableValue
        }

        // Update options
        if let optionsValue = properties["options"] as? [[String: Any]] {
            options = optionsValue
            // Reset filtered options when options change
            filteredOptions = options
        }

        // Update orientation (from styles.base.orientation)
        if let styles = properties["styles"] as? [String: Any],
           let orientationValue = styles["orientation"] as? String {
            orientation = orientationValue
        }

        // Show or hide the pre-created search input based on filterable state
        if filterable {
            searchInputView?.isHidden = false
        } else {
            searchInputView?.isHidden = true
            noResultsLabel?.isHidden = true
            searchText = ""
            filteredOptions = options
        }

        // Recreate options view (uses filtered list if filterable)
        recreateOptions()

        // Update no-results label visibility based on current filtered list
        if filterable && filteredOptions.isEmpty && !options.isEmpty {
            noResultsLabel?.isHidden = false
        } else {
            noResultsLabel?.isHidden = true
        }

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

            if displayStyle == "chips" {
                chipButtons.forEach { button in
                    button.isEnabled = isEnabled
                    button.alpha = alpha
                }
            } else {
                optionButtons.forEach { button in
                    button.isEnabled = isEnabled
                    button.alpha = alpha
                }
            }
        }

        setNeedsLayout()
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
        // Clear existing button references
        optionButtons.removeAll()
        chipButtons.removeAll()
        selectedRadioIndex = nil

        guard let optionsContainer = optionsContainer else { return }

        // Remove all subviews from options container (only buttons live here)
        optionsContainer.subviews.forEach { $0.removeFromSuperview() }

        // Create options based on displayStyle, using filtered options when filterable
        if displayStyle == "chips" {
            createChips(in: optionsContainer)
        } else {
            createOptions(in: optionsContainer)
        }

        setNeedsLayout()
    }

    /// Create options (single and multi selection both use CheckBoxButton)
    private func createOptions(in container: UIView) {
        // Use filtered options when filterable, otherwise use all options
        let optionsToDisplay = filterable ? filteredOptions : options

        for (index, option) in optionsToDisplay.enumerated() {
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
    }

    /// Create chips style options (always use row-wrap layout regardless of orientation)
    private func createChips(in container: UIView) {
        // Use filtered options when filterable, otherwise use all options
        let optionsToDisplay = filterable ? filteredOptions : options

        for (index, option) in optionsToDisplay.enumerated() {
            let label = extractTextValue(option["label"])
            let value = option["value"] as? String ?? ""

            let chipButton = ChipButton()
            chipButton.setTitle(label, for: .normal)
            chipButton.value = value
            chipButton.tag = index

            // Apply chip style configuration (defaults match diff)
            chipButton.cornerRadius = 16
            chipButton.borderWidth = 1.0
            chipButton.paddingHorizontal = 16
            chipButton.paddingVertical = 8
            chipButton.selectedBackgroundColor = selectedBackgroundColor
            chipButton.selectedTextColor = .white
            chipButton.unselectedBackgroundColor = .clear
            chipButton.unselectedBorderColor = UIColor.black.withAlphaComponent(0.1)
            chipButton.unselectedTextColor = textColor

            chipButton.addTarget(self, action: #selector(chipButtonTapped(_:)), for: .touchUpInside)

            chipButtons.append(chipButton)
            container.addSubview(chipButton)
        }
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

    // MARK: - Private Methods - Search

    /// Handle search text change with debounce
    private func handleSearchTextChanged(_ text: String) {
        searchText = text

        // Simple debounce using DispatchQueue
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            guard let self = self else { return }
            // Only filter if the search text hasn't changed during debounce
            if self.searchText == text {
                self.filterOptions()
            }
        }
    }

    /// Filter options based on search text
    private func filterOptions() {
        if searchText.isEmpty {
            filteredOptions = options
        } else {
            let lowercasedSearchText = searchText.lowercased()
            filteredOptions = options.filter { option in
                let label = extractTextValue(option["label"])
                return label.lowercased().contains(lowercasedSearchText)
            }
        }

        // Recreate options with filtered list
        recreateOptions()

        // Show/hide no results label
        if filteredOptions.isEmpty && !options.isEmpty {
            noResultsLabel?.isHidden = false
        } else {
            noResultsLabel?.isHidden = true
        }

        setNeedsLayout()
    }

    // MARK: - Private Methods - Value Update

    /// Update selected value
    private func updateSelectedValue(_ value: Any) {
        if displayStyle == "chips" {
            // Chips style
            if variant == "mutuallyExclusive" {
                updateChipRadioSelection(value as? String)
            } else {
                updateChipCheckBoxSelection(value as? [String] ?? [])
            }
        } else {
            // Checkbox style
            if variant == "mutuallyExclusive" {
                updateRadioSelection(value as? String)
            } else {
                updateCheckBoxSelection(value as? [String] ?? [])
            }
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

    /// Update chip radio button selected state (single selection)
    private func updateChipRadioSelection(_ selectedValue: String?) {
        for button in chipButtons {
            button.isSelected = button.value == selectedValue
        }
    }

    /// Update chip checkbox selected state (multi selection)
    private func updateChipCheckBoxSelection(_ selectedValues: [String]) {
        for button in chipButtons {
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

        if let str = value as? String {
            return str
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

    /// Chip button tap handler (works for both single and multi selection)
    @objc private func chipButtonTapped(_ sender: ChipButton) {
        guard !isUpdatingFromNative else { return }

        if variant == "mutuallyExclusive" {
            // Single selection mode: only one can be selected
            for button in chipButtons {
                button.isSelected = (button == sender)
            }

            // Send data change as array (catalog requires DynamicStringList)
            syncState(["value": [sender.value]])
        } else {
            // Multi selection mode: toggle current chip
            sender.isSelected = !sender.isSelected

            // Collect all selected values
            var selectedValues: [String] = []
            for button in chipButtons {
                if button.isSelected {
                    selectedValues.append(button.value)
                }
            }

            syncState(["value": selectedValues])
        }
    }
}

#endif // AGENUI_SDK_BUILD