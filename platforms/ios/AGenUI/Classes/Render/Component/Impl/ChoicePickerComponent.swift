//
//  ChoicePickerComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit
#if ENABLE_CUSTOM_YOGA
#else
import FlexLayout
#endif

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
        textField.placeholder = "搜索选项..."
        textField.font = UIFont.systemFont(ofSize: fontSize)
        textField.textColor = textColor
        textField.borderStyle = .none
        textField.delegate = self
        
        // Configure clear button
        clearButton.setImage(UIImage(systemName: "xmark.circle.fill"), for: .normal)
        clearButton.tintColor = iconColor
        clearButton.addTarget(self, action: #selector(clearTapped), for: .touchUpInside)
        clearButton.isHidden = true
        
        // Add subviews using FlexLayout
        flex.direction(.row).alignItems(.center).paddingHorizontal(padding)
        
        flex.addItem(searchIconImageView).width(iconSize).height(iconSize)
        flex.addItem(textField).grow(1).shrink(1).marginLeft(padding / 2)
        flex.addItem(clearButton).width(iconSize).height(iconSize).marginLeft(padding / 2)
        
        // Apply styles
        layer.cornerRadius = cornerRadius
        layer.borderColor = borderColor.cgColor
        layer.borderWidth = borderWidth
        backgroundColor = viewBackgroundColor
    }
    
    // MARK: - Actions
    
    @objc private func clearTapped() {
        textField.text = ""
        clearButton.isHidden = true
        onTextChanged?("")
    }
    
    // MARK: - Public Methods
    
    func setText(_ text: String) {
        textField.text = text
        clearButton.isHidden = text.isEmpty
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
        clearButton.isHidden = text.isEmpty
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
///
/// Design notes:
/// - Uses CheckBoxButton for checkbox display style
/// - Uses ChipButton for chips display style
/// - Single selection acts as radio buttons (only one can be selected)
/// - Supports vertical and horizontal layout orientations
class ChoicePickerComponent: Component {
    
    // MARK: - Properties
    
    private var optionsContainer: UIView?
    private var errorLabel: UILabel?
    private var isUpdatingFromNative = false
    
    private var variant: String = "mutuallyExclusive" // Default single selection
    private var displayStyle: String = "checkbox" // Default checkbox style
    private var filterable: Bool = false // Default not filterable
    private var options: [[String: Any]] = []
    private var orientation: String = "vertical" // Default vertical layout
    
    // Search state
    private var searchText: String = ""
    private var filteredOptions: [[String: Any]] = []
    
    // Option buttons for checkbox style
    private var optionButtons: [CheckBoxButton] = []
    private var selectedRadioIndex: Int?
    
    // Option buttons for chips style
    private var chipButtons: [ChipButton] = []
    
    // Search input view
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
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "ChoicePicker", properties: properties)
        
        // Load style configuration
        loadLocalStyleConfig()
        
        // Pre-create search input and add it first (hidden by default).
        // This ensures it always appears above the options container regardless
        // of when filterable is toggled, since FlexLayout has no insertItem API.
        let searchView = SearchInputView()
        searchView.onTextChanged = { [weak self] text in
            self?.handleSearchTextChanged(text)
        }
        self.searchInputView = searchView
        flex.addItem(searchView).display(.none).marginHorizontal(8).marginTop(8).marginBottom(0).height(44)
        
        // Create no results label (hidden by default)
        let noResultsLabel = UILabel()
        noResultsLabel.text = "No matching options"
        noResultsLabel.font = UIFont.systemFont(ofSize: 14)
        noResultsLabel.textColor = UIColor.black.withAlphaComponent(0.5)
        noResultsLabel.textAlignment = .center
        self.noResultsLabel = noResultsLabel
        flex.addItem(noResultsLabel).display(.none).marginHorizontal(8).marginTop(8).marginBottom(8)
        
        // Create options container
        let optionsView = UIView()
        self.optionsContainer = optionsView
        flex.addItem(optionsView).direction(.column).paddingHorizontal(8).paddingTop(4).paddingBottom(8)
        
        // Create error label
        createErrorLabel()
        
        // Apply initial properties
        updateProperties(properties)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Component Override
    
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
            searchInputView?.flex.display(.flex)
        } else {
            searchInputView?.flex.display(.none)
            noResultsLabel?.flex.display(.none)
            searchText = ""
            filteredOptions = options
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
        chipButtons.removeAll()
        selectedRadioIndex = nil
        
        guard let optionsContainer = optionsContainer else { return }
        
        // Remove all subviews except search input and no results label
        optionsContainer.subviews.forEach { view in
            if view !== searchInputView && view !== noResultsLabel {
                view.removeFromSuperview()
            }
        }
        
        // Create options based on displayStyle using filtered options
        if displayStyle == "chips" {
            createChips(in: optionsContainer)
        } else {
            createOptions(in: optionsContainer)
        }
        
        optionsContainer.flex.layout(mode: .adjustHeight)
    }
    
    /// Create options (single and multi selection both use CheckBoxButton)
    private func createOptions(in container: UIView) {
        // Set layout direction based on orientation
        let flexDirection: Flex.Direction = (orientation == "horizontal") ? .row : .column
        
        // Use filtered options if filterable, otherwise use all options
        let optionsToDisplay = filterable ? filteredOptions : options
        
        container.flex.direction(flexDirection).define { flex in
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
                    // Single selection mode
                    button.addTarget(self, action: #selector(radioButtonTapped(_:)), for: .touchUpInside)
                } else {
                    // Multi selection mode
                    button.addTarget(self, action: #selector(checkBoxButtonTapped(_:)), for: .touchUpInside)
                }
                
                optionButtons.append(button)
                
                // Set different gaps based on layout direction (use configured choiceGap)
                let halfGap = choiceGap / 2
                if orientation == "horizontal" {
                    flex.addItem(button).marginHorizontal(halfGap).height(40).width(100)
                } else {
                    flex.addItem(button).marginVertical(halfGap).height(40).width(100%)
                }
            }
        }
    }
    
    /// Create chips style options
    private func createChips(in container: UIView) {
        // Chips always use vertical layout with wrapping
        container.flex.direction(.column).define { flex in
            // Create a horizontal flow container for chips
            let flowContainer = UIView()
            flowContainer.flex.direction(.row).wrap(.wrap).justifyContent(.start)
            
            // Use filtered options if filterable, otherwise use all options
            let optionsToDisplay = self.filterable ? self.filteredOptions : self.options
            
            for (index, option) in optionsToDisplay.enumerated() {
                let label = self.extractTextValue(option["label"])
                let value = option["value"] as? String ?? ""
                
                let chipButton = ChipButton()
                chipButton.setTitle(label, for: .normal)
                chipButton.value = value
                chipButton.tag = index
                
                // Apply chip style configuration
                chipButton.cornerRadius = 16
                chipButton.borderWidth = 1.0
                chipButton.paddingHorizontal = 16
                chipButton.paddingVertical = 8
                chipButton.selectedBackgroundColor = self.selectedBackgroundColor
                chipButton.selectedTextColor = .white
                chipButton.unselectedBackgroundColor = .clear
                chipButton.unselectedBorderColor = UIColor.black.withAlphaComponent(0.1)
                chipButton.unselectedTextColor = self.textColor
                
                if self.variant == "mutuallyExclusive" {
                    // Single selection mode
                    chipButton.addTarget(self, action: #selector(self.chipButtonTapped(_:)), for: .touchUpInside)
                } else {
                    // Multi selection mode
                    chipButton.addTarget(self, action: #selector(self.chipButtonTapped(_:)), for: .touchUpInside)
                }
                
                self.chipButtons.append(chipButton)
                
                // Add chip to flow container with gap
                let gap = self.choiceGap
                flowContainer.flex.addItem(chipButton).margin(gap / 2).height(36)
            }
            
            flex.addItem(flowContainer).width(100%).paddingHorizontal(8).paddingVertical(4)
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
        flex.addItem(label).marginHorizontal(8).marginTop(4).marginBottom(8)
    }
    
    /// Search input is now pre-created in init. This method is intentionally left empty.
    private func createSearchInput() {}
    
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
                guard let label = option["label"] as? String else { return false }
                let lowercasedLabel = label.lowercased()
                return lowercasedLabel.contains(lowercasedSearchText)
            }
        }
        
        // Recreate options with filtered list
        recreateOptions()
        
        // Show/hide no results label
        if filteredOptions.isEmpty {
            noResultsLabel?.flex.display(.flex)
        } else {
            noResultsLabel?.flex.display(.none)
        }
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
                // Single selection mode
                updateRadioSelection(value as? String)
            } else {
                // Multi selection mode
                updateCheckBoxSelection(value as? [String] ?? [])
            }
        }
    }
    
    /// Update chip radio button selected state (single selection)
    private func updateChipRadioSelection(_ selectedValue: String?) {
        for (index, button) in chipButtons.enumerated() {
            let isSelected = button.value == selectedValue
            button.isSelected = isSelected
        }
    }
    
    /// Update chip checkbox selected state (multi selection)
    private func updateChipCheckBoxSelection(_ selectedValues: [String]) {
        for button in chipButtons {
            button.isSelected = selectedValues.contains(button.value)
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
        optionsContainer?.flex.layout(mode: .adjustHeight)
    }
    
    /// Hide error message
    private func hideError() {
        errorLabel?.text = nil
        errorLabel?.isHidden = true
        optionsContainer?.flex.layout(mode: .adjustHeight)
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
