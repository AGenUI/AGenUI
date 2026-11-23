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
    /// fileprivate: HorizontalCollectionView (same file) reads it in scrollViewDidScroll.
    fileprivate var isHorizontalList: Bool { direction == .horizontal }

    /// Envelope writing direction ("rtl" | "ltr"/absent), injected by Surface
    /// into properties. Stored (not computed), parsed together with direction
    /// in parseProperties — so updateProperties can snapshot the old value
    /// before parsing, exactly like the direction flip detection above.
    fileprivate var isRTL = false

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
    
    override func updateProperties(_ diff: [String: DiffValue]) {
        // Snapshot the stored state before parsing the merged properties —
        // same pattern as direction below, so a flip is detectable.
        let oldDirection = direction
        let oldIsRTL = isRTL

        // Call parent method to apply CSS properties to self
        super.updateProperties(diff)
        
        // Parse properties
        parseProperties()
        
        // Reconfigure layout if direction changed
        if oldDirection != direction {
            collectionView.configureLayout()
        }
        
        // Direction flip: force a re-read of the Yoga frames. The mirrored
        // child styles arrive as per-child updates that re-fire onFrameChange,
        // which re-anchors the start edge as the mirrored content lands.
        if oldIsRTL != isRTL, isHorizontalList {
            collectionView.refreshHorizontalLayout()
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
        
        // Parse writing direction (envelope injected by Surface)
        isRTL = (properties["writingDirection"] as? String)?.lowercased() == "rtl"
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
        let listPadding = CSSPaddingResolver.resolve(listStyles, isRTL: listComponent.isRTL)

        // RTL normalization: Yoga mirrors children inside the container width,
        // so content wider than one screen extends to NEGATIVE x. UIScrollView
        // cannot scroll into negative territory, and contentSize below only
        // tracks the positive span — which froze RTL scrolling (contentSize ≈
        // viewport, maxOffset = 0). Shift every frame so the leftmost content
        // sits at the LEFT PADDING edge, not at x = 0: the plain max(0, -minX)
        // shift glued the last child to the screen edge and dropped the
        // physical-left (= logical inline-end) gutter from the scroll content.
        // Non-overflowing RTL keeps minX >= listPadding.left, so the shift
        // evaluates to 0 there; LTR is unchanged.
        let minX = frames.map { $0.minX }.min() ?? 0
        let rtlShift = max(0, -minX + (listComponent.isRTL ? listPadding.left : 0))

        for (index, yogaFrame) in frames.enumerated() {
            let attrs = UICollectionViewLayoutAttributes(forCellWith: IndexPath(item: index, section: 0))
            attrs.frame = CGRect(
                x: yogaFrame.minX + rtlShift,
                y: yogaFrame.minY,
                width: yogaFrame.width,
                height: yogaFrame.height
            )
            cachedAttributes.append(attrs)

            let rightExtent = attrs.frame.maxX + listPadding.right
            if rightExtent > contentBounds.width {
                contentBounds.width = rightExtent
            }
            let bottomExtent = yogaFrame.maxY + listPadding.bottom
            if bottomExtent > contentBounds.height {
                contentBounds.height = bottomExtent
            }
        }
    }


    /// Clamp content height to no more than the viewport height, to prevent vertical scrolling.
    override var collectionViewContentSize: CGSize {
        var size = contentBounds
        if let viewportHeight = collectionView?.bounds.height, viewportHeight > 0 {
            size.height = min(size.height, viewportHeight)
        }
        return size
    }
    
    /// Returns cached attributes for elements in the visible rect
    override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]? {
        return cachedAttributes.filter { attr in attr.frame.intersects(rect) }
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

    /// True while addHorizontalChild runs its synchronous refresh pass:
    /// reloadData → cell attach writes child.frame → the onFrameChange
    /// handler fires nested inside this pass. The outer alignToLogicalStart
    /// right after the refresh already re-anchors with the final contentSize,
    /// so the handler skips its own forced relayout to avoid a nested
    /// layoutIfNeeded and a doubled (stale-contentSize) offset write. Only
    /// this call site sets the flag — the direction-flip path relies on the
    /// handler's own re-anchor and must keep it.
    private var isRefreshingHorizontalLayout = false

    /// Geometry snapshot from the last layoutSubviews re-anchor pass.
    /// contentOffset IS bounds.origin, so scrolling fires layoutSubviews
    /// every frame — and isTracking/isDecelerating leave gaps (finger-lift
    /// frame before deceleration starts; animated setContentOffset where
    /// both are false). Instead of scroll-state flags, anchor only when the
    /// geometry that defines maxOffset itself changed: viewport size or
    /// content size. Scrolling never changes either, so every user-driven
    /// frame — tracking, decelerating, and the gaps between — is silent
    /// by construction.
    private var lastAnchorViewportSize: CGSize = .zero
    private var lastAnchorContentSize: CGSize = .zero
    
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
    
    /// Re-anchor the offset to the logical start edge: RTL start is the
    /// physical right end (maxOffset), LTR start is 0. Called on every content
    /// change — child arrival (addHorizontalChild) and child frame updates
    /// (onFrameChange) — after a synchronous layout pass so contentSize is
    /// fresh. Scrolling never triggers it: scrolling moves cells, not child
    /// frames.
    func alignToLogicalStart() {
        guard let owner, bounds.width > 0 else { return }
        let maxOffset = max(0, contentSize.width - bounds.width)
        contentOffset.x = owner.isRTL ? maxOffset : 0
    }
    
    /// After a direction flip UIKit animates the offset back to its
    /// anchor-restoration target, walking it off the logical start edge.
    /// That animation ignores plain model-layer writes, but an explicit
    /// setContentOffset(_:animated:) call stops it — so any external push
    /// while no user interaction is in flight (tracking / decelerating)
    /// snaps the offset back to the logical start edge.
    func scrollViewDidScroll(_ scrollView: UIScrollView) {
        guard let owner, owner.isHorizontalList,
              !isTracking, !isDecelerating,
              bounds.width > 0 else { return }

        let target = owner.isRTL ? max(0, contentSize.width - bounds.width) : 0
        guard abs(scrollView.contentOffset.x - target) > 0.5 else { return }
        setContentOffset(CGPoint(x: target, y: scrollView.contentOffset.y), animated: false)
    }

    /// Re-anchor after ANY size change, not just content changes.
    ///
    /// Every size change — first-frame width arrival, sheet width animation,
    /// contentSize growth from late-arriving children — runs a layout pass
    /// that lands here with bounds and contentSize both fresh (super already
    /// ran prepare()). alignToLogicalStart cannot cover these: on the first
    /// frame every call runs before the collection view has any width, and
    /// the guard drops the request with no retry, leaving the offset at 0 =
    /// the RTL logical END. Anchoring is keyed on geometry change (see
    /// lastAnchorViewportSize) so user scrolling — which reproduces this
    /// callback every frame but never changes size or contentSize — never
    /// re-anchors. RTL start is the physical right end (maxOffset); LTR
    /// start is 0 and is left untouched — snapping LTR here would introduce
    /// new behavior (idle layout passes pulling a browsing user back to the
    /// head). Geometry changes that land mid-gesture stay pending: the
    /// snapshot only advances on an actual write, so the first post-gesture
    /// layout pass completes the deferred anchor.
    override func layoutSubviews() {
        super.layoutSubviews()
        guard let owner, owner.isHorizontalList, owner.isRTL,
              bounds.width > 0 else { return }
        guard bounds.size != lastAnchorViewportSize
                || contentSize != lastAnchorContentSize else { return }
        // Mid-gesture (tracking / decelerating, e.g. sheet width animating
        // under a dragging finger): return WITHOUT advancing the snapshot —
        // the geometry diff stays pending, so the next post-gesture layout
        // pass (deceleration settle frames, any later invalidation) still
        // sees the diff and completes the anchor. Advancing the snapshot
        // here would swallow the diff and drop the re-anchor permanently.
        guard !isTracking, !isDecelerating else { return }
        lastAnchorViewportSize = bounds.size
        lastAnchorContentSize = contentSize
        contentOffset.x = max(0, contentSize.width - bounds.width)
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
            // Nested inside addHorizontalChild's refresh (reloadData → attach
            // → frame reset): the outer refresh + alignToLogicalStart below
            // already relayout and re-anchor with the final contentSize.
            guard !self.isRefreshingHorizontalLayout else { return }
            collectionViewLayout.invalidateLayout()
            // Synchronous relayout so contentSize reflects the new frame, then
            // re-anchor to the logical start edge.
            layoutIfNeeded()
            alignToLogicalStart()
        }
        isRefreshingHorizontalLayout = true
        refreshHorizontalLayout()
        isRefreshingHorizontalLayout = false
        alignToLogicalStart()
    }
    
    func configureLayout() {
        isScrollEnabled = true
        alwaysBounceVertical = false
        alwaysBounceHorizontal = false
   }
    

}
