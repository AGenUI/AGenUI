//
//  ListComponent.swift
//  AGenUI
//
// Created on 2026/3/3.
//

import UIKit

/// ListComponent component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - direction: Layout direction (String: vertical, horizontal)
/// - align: Cross axis alignment (String: start, center, end, stretch)
/// - spacing: Child component spacing (Double, default 0)
/// - children: Child component ID array (Array<String>)
///
/// Design notes:
/// - Horizontal uses UICollectionView with YogaCollectionViewLayout for lazy rendering
/// - Vertical uses direct child subviews created by the base Component path
/// - Vertical direction: scrolling disabled; Horizontal: scrolling enabled
/// - Yoga-computed frames are reused for both paths
class ListComponent: Component {

    // MARK: - Properties

    private var direction: ListDirection = .vertical
    private var align: String = "start"

    /// Whether the list renders horizontally via UICollectionView with lazy cell recycling.
    private var isHorizontalList: Bool { direction == .horizontal }

    /// Guard flag to prevent recursive onFrameChange handling when we reset child origin.
    private var isResettingChildOrigin = false

    private let yogaLayout = YogaCollectionViewLayout()
    
    private lazy var collectionView = HorizontalCollectionView(layout: yogaLayout)

    // MARK: - Enums

    enum ListDirection: String {
        case vertical = "vertical"
        case horizontal = "horizontal"
    }

    // MARK: - Initialization

    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "List", properties: properties)
        createView()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    //MARK: - Component Override
    
    override func createView(){
        // Parse direction before choosing the backing view for the initial render.
        parseProperties()
        if isHorizontalList {
            collectionView.bind(self)
            addSubview(collectionView)
            collectionView.configureLayout()
        }
        super.createView()
    }
    
    override func updateProperties(_ properties: [String: Any]) {
        // Call parent method to apply CSS properties to self
        super.updateProperties(properties)
        
        // Parse properties
        let oldDirection = direction
        parseProperties()
        
        // Reconfigure layout if direction changed
        if oldDirection != direction {
            collectionView.configureLayout()
        }
    }
    
    // MARK: - Private Methods
    
    /// Parse component properties
    private func parseProperties() {
        // Parse direction
        if let directionStr = properties["direction"] as? String,
           let dir = ListDirection(rawValue: directionStr) {
            direction = dir
        }
        
        // Parse align
        if let alignValue = properties["align"] as? String {
            align = alignValue
        }
    }


    // MARK: - Child Management

    override func addChild(_ child: Component) {
        super.addChild(child)

        guard isHorizontalList else { return }
        collectionView.addHorizontalChild(child)
    }

    override func removeChild(_ child: Component) {
        child.onFrameChange = nil
        super.removeChild(child)
        guard isHorizontalList else { return }
        collectionView.refreshHorizontalLayout()
    }

    // MARK: - Layout

    override func layoutSubviews() {
        super.layoutSubviews()
        guard isHorizontalList else { return }
        collectionView.frame = bounds
    }
    
    override func shouldCreateChildView() -> Bool {
        
        return !isHorizontalList
    }

}

// MARK: - YogaCollectionViewLayout

/// Custom layout that reads pre-computed Yoga frames stored in ListComponent.childYogaFrames.
/// Pure lookup — no layout calculation, Yoga owns all positioning.
private class YogaCollectionViewLayout: UICollectionViewLayout {

    private var cachedAttributes: [UICollectionViewLayoutAttributes] = []
    private var contentBounds: CGSize = .zero

    override func prepare() {
        cachedAttributes.removeAll()
        contentBounds = .zero

        guard let listComponent = collectionView?.superview as? ListComponent else { return }

        let frames: [CGRect] = listComponent.children.map { child in
            guard let styles = child.properties["styles"] as? [String: Any] else { return .zero }
            let x = CGFloat((styles["x"] as? Double) ?? 0) * Component.BS_POINT_SCALE
            let y = CGFloat((styles["y"] as? Double) ?? 0) * Component.BS_POINT_SCALE
            let w = child.frame.width
            let h = child.frame.height
            return CGRect(x: x, y: y, width: w, height: h)
        }

        // Resolve the List's own CSS padding. Yoga already bakes padding-left /
        // padding-top into each child's x / y, so the left/top gutter is covered
        // by child placement. Padding-right / padding-bottom, however, must be
        // appended to the scrollable content size — otherwise the last child
        // sits flush against the right/bottom edge of the scroll area and the
        // right/bottom padding gutter visually disappears.
        let listStyles = (listComponent.properties["styles"] as? [String: Any]) ?? [:]
        let listPadding = CSSPaddingResolver.resolve(listStyles)

        for (index, yogaFrame) in frames.enumerated() {
            let attrs = UICollectionViewLayoutAttributes(forCellWith: IndexPath(item: index, section: 0))
            attrs.frame = yogaFrame
            cachedAttributes.append(attrs)

            let rightExtent = yogaFrame.maxX + listPadding.right
            if rightExtent > contentBounds.width {
                contentBounds.width = rightExtent
            }
            let bottomExtent = yogaFrame.maxY + listPadding.bottom
            if bottomExtent > contentBounds.height {
                contentBounds.height = bottomExtent
            }
        }
    }

    /// Total scrollable content size — union of all child Yoga frames
    override var collectionViewContentSize: CGSize {
        return contentBounds
    }
    
    /// Returns cached attributes for elements in the visible rect
    override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]? {
        return  cachedAttributes.filter { attr in attr.frame.intersects(rect) }
    }
    
    /// Returns the layout attributes for a specific item at indexPath
    override func layoutAttributesForItem(at indexPath: IndexPath) -> UICollectionViewLayoutAttributes? {
        guard indexPath.item < cachedAttributes.count else { return nil }
        return cachedAttributes[indexPath.item]
    }
    
    /// Whether layout should be invalidated when collection view bounds change
    override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
        return true
    }
}

// MARK: - ComponentCell

/// Thin cell wrapper — holds a Component as a subview of contentView.
private class ComponentCell: UICollectionViewCell {
    static let reuseId = "ComponentCell"

    private weak var currentComponent: Component?

    func attach(_ component: Component) {
        if currentComponent === component { return }
        if currentComponent?.superview === contentView {
            // Only remove the view if this cell still owns it.
            currentComponent?.removeFromSuperview()
        }

        currentComponent = component
        // Reuse only resets the cell-local origin; Yoga-computed size stays intact.
        component.frame = CGRect(origin: .zero, size: component.frame.size)
        contentView.addSubview(component)
    }

}

/// Dedicated container for horizontal lists. Vertical lists do not use it.
private  class HorizontalCollectionView: UICollectionView, UICollectionViewDelegate, UICollectionViewDataSource{
    private weak var owner: ListComponent?
    private var isResettingChildOrigins = false
    
    init(layout: UICollectionViewLayout) {
        super.init(frame: .zero, collectionViewLayout: layout)
        backgroundColor = .clear
        showsVerticalScrollIndicator = false
        showsHorizontalScrollIndicator = false
        isPrefetchingEnabled = false
        register(ComponentCell.self, forCellWithReuseIdentifier: ComponentCell.reuseId)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    func bind(_ owner: ListComponent) {
        self.owner = owner
        dataSource = self
        delegate = self
    }
    
    func refreshHorizontalLayout(){
        collectionViewLayout.invalidateLayout()
        reloadData()
        layoutIfNeeded()
    }
    
    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return owner?.children.count ?? 0
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        guard let owner else{
            return UICollectionViewCell()
        }
        let cell = collectionView.dequeueReusableCell(withReuseIdentifier: ComponentCell.reuseId, for: indexPath) as! ComponentCell
        let child = owner.children[indexPath.item]
        cell.attach(child)
        child.createView()
        if !child.componentId.isEmpty  {
            child.notifyAppeared()
        }
        return cell
    }
    
    func addHorizontalChild(_ child:Component){
        
        child.onFrameChange = { [weak self, weak child] newFrame in
            guard let self, let child, !self.isResettingChildOrigins else { return }
            self.isResettingChildOrigins = true
            child.frame = CGRect(origin: .zero, size: newFrame.size)
            self.isResettingChildOrigins = false
            collectionViewLayout.invalidateLayout()
            layoutIfNeeded()
        }
        refreshHorizontalLayout()
    }
    
    func configureLayout() {
        isScrollEnabled = true
        alwaysBounceVertical = false
        alwaysBounceHorizontal = false
   }
    

}
