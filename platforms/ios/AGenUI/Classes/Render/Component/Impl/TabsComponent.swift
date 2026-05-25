//
//  TabsComponent.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// Tab data model
struct TabItem {
    let title: String
    let childId: String?
}

/// Inner Tab view, manages TabBar and content area
class InnerTabsView: UIView {

    // MARK: - Properties

    private var tabBarView: UIView!
    private var contentView: UIView!
    private var indicatorView: UIView!
    private var tabButtons: [UIButton] = []
    private(set) var selectedIndex: Int = 0

    /// Store all content components, keyed by childId
    private var contentComponents: [String: UIView] = [:]

    /// Layout change callback (set by TabsComponent)
    var onLayoutChanged: (() -> Void)?

    /// Tab selection callback, provides selected index
    var onTabSelected: ((Int) -> Void)?

    /// Tab data
    private var tabItems: [TabItem] = []

    /// Style properties
    var indicatorColor: UIColor = UIColor(red: 0x22/255.0, green: 0x73/255.0, blue: 0xF7/255.0, alpha: 1.0)
    var indicatorWidth: CGFloat = 24
    var indicatorHeight: CGFloat = 4
    var indicatorRadius: CGFloat = 2
    var selectedTabColor: UIColor = UIColor(red: 0x22/255.0, green: 0x73/255.0, blue: 0xF7/255.0, alpha: 1.0)
    var normalTabColor: UIColor = .black
    var fontSize: CGFloat = 16
    var fontSizeSelected: CGFloat = 16
    var fontWeight: UIFont.Weight = .medium
    var fontWeightSelected: UIFont.Weight = .bold

    // MARK: - Initialization

    override init(frame: CGRect) {
        super.init(frame: frame)
        setupSubviews()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupSubviews()
    }

    // MARK: - Setup

    private func setupSubviews() {
        // TabBar container
        tabBarView = UIView()
        tabBarView.backgroundColor = .clear
        tabBarView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(tabBarView)
        NSLayoutConstraint.activate([
            tabBarView.topAnchor.constraint(equalTo: topAnchor),
            tabBarView.leadingAnchor.constraint(equalTo: leadingAnchor),
            tabBarView.trailingAnchor.constraint(equalTo: trailingAnchor),
            tabBarView.heightAnchor.constraint(equalToConstant: 44)
        ])

        // Content container
        contentView = UIView()
        contentView.backgroundColor = .clear
        contentView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(contentView)
        NSLayoutConstraint.activate([
            contentView.topAnchor.constraint(equalTo: tabBarView.bottomAnchor),
            contentView.leadingAnchor.constraint(equalTo: leadingAnchor),
            contentView.trailingAnchor.constraint(equalTo: trailingAnchor),
            contentView.bottomAnchor.constraint(equalTo: bottomAnchor)
        ])

        // Indicator
        indicatorView = UIView()
        indicatorView.backgroundColor = indicatorColor
        indicatorView.layer.cornerRadius = indicatorRadius
    }

    override func layoutSubviews() {
        super.layoutSubviews()

        guard bounds.width > 0 else { return }

        // Layout tab buttons evenly across tabBarView
        if !tabButtons.isEmpty, tabBarView.bounds.width > 0 {
            let buttonWidth = tabBarView.bounds.width / CGFloat(tabButtons.count)
            let buttonHeight = tabBarView.bounds.height
            for (index, button) in tabButtons.enumerated() {
                button.frame = CGRect(
                    x: buttonWidth * CGFloat(index),
                    y: 0,
                    width: buttonWidth,
                    height: buttonHeight
                )
            }
        }

        // Ensure indicator position updates after layout completes
        if !tabButtons.isEmpty {
            updateIndicatorPosition(animated: false)
        }

        // Sync each content component frame to contentView bounds
        for view in contentView.subviews {
            view.frame = contentView.bounds
        }
    }

    private func layoutTabButtons() {
        guard !tabButtons.isEmpty else { return }

        for button in tabButtons {
            tabBarView.addSubview(button)
        }
    }

    private func updateIndicatorPosition(animated: Bool) {
        guard !tabButtons.isEmpty, selectedIndex < tabButtons.count, tabBarView.bounds.width > 0 else { return }

        // Calculate indicator position using frame-based layout
        let buttonWidth = tabBarView.bounds.width / CGFloat(tabButtons.count)
        let indicatorLeft = buttonWidth * CGFloat(selectedIndex) + (buttonWidth - indicatorWidth) / 2

        let targetFrame = CGRect(
            x: indicatorLeft,
            y: tabBarView.bounds.height - indicatorHeight,
            width: indicatorWidth,
            height: indicatorHeight
        )

        if indicatorView.superview == nil {
            tabBarView.addSubview(indicatorView)
        }

        if animated {
            UIView.animate(withDuration: 0.3, delay: 0, usingSpringWithDamping: 0.8, initialSpringVelocity: 0.5, options: [.curveEaseInOut, .allowUserInteraction], animations: {
                self.indicatorView.frame = targetFrame
            })
        } else {
            indicatorView.frame = targetFrame
        }
    }

    // MARK: - Public Methods

    /// Set Tab data, recreate TabBar if data changed
    func setTabs(_ items: [TabItem]) {
        // Check if data actually changed
        let currentTitles = tabItems.map { $0.title }
        let newTitles = items.map { $0.title }

        guard newTitles != currentTitles else {
            return
        }

        // Delete old TabBar
        tabButtons.forEach { $0.removeFromSuperview() }
        tabButtons.removeAll()
        indicatorView.removeFromSuperview()

        // Update data
        tabItems = items

        // Recreate TabBar
        setupTabBar()

        // Try to show currently selected content component (may have been added before setTabs)
        showCurrentContent()
    }

    /// Add content component
    /// - Parameters:
    ///   - componentId: Component ID (corresponds to TabItem.childId)
    ///   - component: Component view
    func addContentComponent(componentId: String, component: UIView) {
        // Store component
        contentComponents[componentId] = component

        // Add to contentView but keep hidden; show/hide is managed by showCurrentContent
        if component.superview == nil {
            contentView.addSubview(component)
        }
        component.isHidden = true

        // Show if this matches the currently selected tab
        showCurrentContent()
    }

    /// Select specified Tab
    func selectTab(at index: Int) {
        guard index >= 0, index < tabButtons.count else { return }

        selectedIndex = index
        updateTabAppearance()
        updateIndicatorPosition(animated: true)
        showCurrentContent()
    }

    func isSelectedChild(_ componentId: String) -> Bool {
        guard selectedIndex < tabItems.count,
              let childId = tabItems[selectedIndex].childId else { return false }
        return childId == componentId
    }

    /// Update styles
    func updateStyles() {
        indicatorView.backgroundColor = indicatorColor
        indicatorView.layer.cornerRadius = indicatorRadius
        updateTabAppearance()
        updateIndicatorPosition(animated: false)
    }

    // MARK: - Private Methods

    /// Show content for the currently selected tab; hide all others
    private func showCurrentContent() {
        // Hide all content components first
        for view in contentComponents.values {
            view.isHidden = true
        }

        // Show the one matching current selection
        guard selectedIndex < tabItems.count,
              let currentChildId = tabItems[selectedIndex].childId,
              let component = contentComponents[currentChildId] else {
            return
        }

        component.isHidden = false
        onLayoutChanged?()
        setNeedsLayout()
    }

    private func setupTabBar() {
        // Create Tab buttons
        for (index, item) in tabItems.enumerated() {
            let button = UIButton(type: .system)
            button.setTitle(item.title, for: .normal)
            button.titleLabel?.font = UIFont.systemFont(ofSize: fontSize, weight: fontWeight)
            button.contentHorizontalAlignment = .center
            button.tag = index
            button.addTarget(self, action: #selector(tabButtonTapped(_:)), for: .touchUpInside)
            tabButtons.append(button)
        }

        layoutTabButtons()
        selectTab(at: 0)
    }

    private func updateTabAppearance() {
        for (index, button) in tabButtons.enumerated() {
            if index == selectedIndex {
                button.setTitleColor(selectedTabColor, for: .normal)
                button.titleLabel?.font = UIFont.systemFont(ofSize: fontSizeSelected, weight: fontWeightSelected)
            } else {
                button.setTitleColor(normalTabColor, for: .normal)
                button.titleLabel?.font = UIFont.systemFont(ofSize: fontSize, weight: fontWeight)
            }
        }
    }

    @objc private func tabButtonTapped(_ sender: UIButton) {
        let index = sender.tag
        selectTab(at: index)

        // Notify tab selection
        onTabSelected?(index)
    }
}

/// Tabs component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - tabs: Tab array, each with title (String) and child (String component reference) (Array)
///
/// Design notes:
/// - Uses InnerTabsView to manage TabBar and content area
/// - Custom Tab buttons with animated indicator
/// - Content components are added via addChild and shown/hidden based on selected tab
open class TabsComponent: Component {

    // MARK: - Properties

    private var innerTabsView: InnerTabsView!

    // MARK: - Initialization

    public init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "Tabs", properties: properties)

        // Create InnerTabsView
        innerTabsView = InnerTabsView()
        innerTabsView.onLayoutChanged = { [weak self] in
            // TODO: notifyRenderFinish
        }
        addSubview(innerTabsView)
        innerTabsView.onTabSelected = { [weak self] index in
            self?.onTabSelected(index: index)
            self?.notifyHeightForSelectedTab()
        }

        updateProperties(properties)
    }

    // MARK: - Tab Selection Hook

    /// Called when a tab is selected by user interaction.
    ///
    /// Subclasses (Swift or Objective-C) can override this method to respond to tab switch events.
    ///
    /// - Parameter index: The index of the selected tab
    @objc open func onTabSelected(index: Int) {
        // Default empty implementation; subclasses override as needed
    }

    required public init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Layout

    open override func layoutSubviews() {
        super.layoutSubviews()
        // Sync innerTabsView frame to component bounds
        innerTabsView.frame = bounds
    }

    // MARK: - Children Management

    /// Get child component ID list from tabs configuration
    /// Parse directly from properties, not relying on tabConfigs (as this method may be called before init)
    open override func getChildrenIdsFromProperties() -> [String] {
        guard let tabs = properties["tabs"] as? [[String: Any]] else {
            return []
        }
        return tabs.compactMap { $0["child"] as? String }
    }

    // MARK: - Component Override

    open override func updateProperties(_ properties: [String: Any]) {
        // Note: children sync is completed in init
        // Only call super here to handle CSS properties etc.
        super.updateProperties(properties)

        // Parse tab text colors from styles
        if let styles = properties["styles"] as? [String: Any] {
            if let selectedTextColorStr = styles["tab-font-color-selected"] as? String,
               let selectedColor = ComponentStyleConfigManager.parseColorToUIColor(selectedTextColorStr) {
                innerTabsView.selectedTabColor = selectedColor
            }
            if let textColorStr = styles["tab-font-color"] as? String,
               let normalColor = ComponentStyleConfigManager.parseColorToUIColor(textColorStr) {
                innerTabsView.normalTabColor = normalColor
            }
            innerTabsView.updateStyles()
        }

        // Update tabs configuration (during dynamic updates)
        if let tabs = properties["tabs"] as? [[String: Any]] {
            // Convert to TabItem array
            let items = tabs.map { tab -> TabItem in
                let title = tab["title"] as? String ?? "Tab"
                let childId = tab["child"] as? String
                return TabItem(title: title, childId: childId)
            }

            // Set Tab data (InnerTabsView auto-handles changes)
            innerTabsView.setTabs(items)
        }
    }

    // MARK: - Children Management

    open override func addChild(_ child: Component) {
        // Call super to maintain children array, parent/surface relationships
        // Note: super.addChild inserts into self.subviews; we then move child into innerTabsView
        super.addChild(child)

        // Remove from self's direct subviews (super added it here) and hand to InnerTabsView
        if child.superview === self {
            child.removeFromSuperview()
        }

        // Register content component with InnerTabsView
        innerTabsView.addContentComponent(componentId: child.componentId, component: child)

        child.frame.origin = .zero

        // Yoga positions Tabs children as absolute (top=48pt) relative to Tabs,
        // but child lives inside contentView which is already below tabBar.
        // Reset origin after each engine layout push to fix the coordinate space mismatch.
        child.onPropertiesUpdate = { [weak self] _ in
            guard let self = self else { return }
            var childFrame = child.frame
            childFrame.origin = .zero
            child.frame = childFrame
            child.isHidden = !self.innerTabsView.isSelectedChild(child.componentId)
            self.notifyHeightForSelectedTab()
        }

        notifyHeightForSelectedTab()
    }

    // MARK: - Height Management

    private func notifyHeightForSelectedTab() {
        guard let surface = surface else { return }
        surface.surfaceManager?.notifyTabSelection(
            surfaceId: surface.surfaceId,
            componentId: componentId,
            type: "TabsIndexChange",
            selectedIndex: innerTabsView.selectedIndex
        )
    }
}
