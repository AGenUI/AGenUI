//
//  TableComponent.swift
//  AGenUI
//
// Created on 2026/3/1.
//

#if AGENUI_SDK_BUILD
import UIKit

// MARK: - TableStyleConfig

/// Table style configuration struct
private struct TableStyleConfig {
    var cellPaddingVertical: CGFloat = 16.0
    var cellPaddingHorizontal: CGFloat = 16.0
    var borderWidth: CGFloat = 1.0
    var borderColor: UIColor = UIColor(red: 0.878, green: 0.878, blue: 0.878, alpha: 1.0)
    var innerBorderColor: UIColor = UIColor(red: 0.882, green: 0.894, blue: 0.914, alpha: 1.0)
    var borderRadius: CGFloat = 0
    var headerBackgroundColor: UIColor = UIColor(red: 0.961, green: 0.961, blue: 0.961, alpha: 1.0)
    var columnWeights: [CGFloat] = []
    var minColumnWidth: CGFloat = 60.0
    var maxColumnWidth: CGFloat = 600.0
    var enableHorizontalScroll: Bool = true
    var configTextAlign: NSTextAlignment = .left
    var configVerticalAlign: String = "center"
    var headerFontColor: UIColor = .black
    var headerFontSize: CGFloat = 14.0
    var headerFontWeight: String = "bold"
    var bodyBgColors: [UIColor] = [.white, UIColor(red: 0.965, green: 0.969, blue: 0.973, alpha: 1.0)]
    var bodyFontColor: UIColor = .black
    var bodyFontSize: CGFloat = 14.0
    var bodyFontWeight: String = "normal"
}

// MARK: - InnerTableView

/// Inner table view, encapsulates table rendering logic
/// Builds table content using frame layout
private class InnerTableView: UIView {
    
    // MARK: - Properties
    
    /// Header data
    var columns: [String] = []
    
    /// Table data
    var rows: [[String]] = []
    
    /// Style configuration
    var styleConfig: TableStyleConfig = TableStyleConfig() {
        didSet { setNeedsLayout() }
    }
    
    /// Border radius - synced from TableComponent when border-radius is applied.
    /// Enables clipsToBounds to clip inner subviews (header/rows) to rounded corners,
    /// while keeping TableComponent itself unclipped so box-shadow remains visible.
    var borderRadius: CGFloat = 0 {
        didSet {
            layer.cornerRadius = borderRadius
            clipsToBounds = borderRadius > 0
        }
    }
    
    /// Horizontal scroll view
    private var scrollView: UIScrollView?
    
    /// Table content container
    private var tableContentView: UIView?
    
    /// Width of each column
    private var columnWidths: [CGFloat] = []
    
    /// Header font
    private var headerFont: UIFont {
        let weight: UIFont.Weight = ComponentStyleConfigManager.parseFontWeight(styleConfig.headerFontWeight)
        return UIFont.systemFont(ofSize: styleConfig.headerFontSize, weight: weight)
    }
    
    /// Data cell font
    private var cellFont: UIFont {
        let weight: UIFont.Weight = ComponentStyleConfigManager.parseFontWeight(styleConfig.bodyFontWeight)
        return UIFont.systemFont(ofSize: styleConfig.bodyFontSize, weight: weight)
    }
    
    /// Minimum row height
    private let minRowHeight: CGFloat = 44.0
    
    // MARK: - Initialization
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    /// Columns and rows assigned; rebuild immediately if width is already known,
    /// otherwise defer until frame is set from outside.
    func reloadData(columns: [String], rows: [[String]], styleConfig: TableStyleConfig) {
        self.styleConfig = styleConfig
        self.columns = columns
        self.rows = rows

        let containerWidth = frame.width
        if containerWidth > 0 {
            buildTableContent(containerWidth: containerWidth)
        }
    }

    // MARK: - Layout

    override var frame: CGRect {
        didSet {
            let newWidth = frame.width
            guard newWidth > 0 else { return }
            buildTableContent(containerWidth: newWidth)
        }
    }
    
    // MARK: - Private Methods

    /// Calculate table height (calculation only, no view creation)
    private func calculateTableHeight(containerWidth: CGFloat) -> CGFloat {
        // If no data, return default height
        guard !columns.isEmpty else { return minRowHeight }

        // 1. Calculate content-based column widths
        let widths = calculateColumnWidths(tableWidth: containerWidth)

        // 2. Calculate header row height
        let headerHeight = calculateRowHeight(texts: columns, columnWidths: widths, isHeader: true)

        // 3. Calculate all data row heights
        var totalHeight = headerHeight
        for rowData in rows {
            let rowHeight = calculateRowHeight(texts: rowData, columnWidths: widths, isHeader: false)
            totalHeight += rowHeight
        }

        return totalHeight
    }

    /// Build table content
    private func buildTableContent(containerWidth: CGFloat) {
        // Clear existing content
        subviews.forEach { $0.removeFromSuperview() }
        scrollView = nil
        tableContentView = nil
        
        // If no data, do not render
        guard !columns.isEmpty else { return }
        
        // 1. First calculate content-based column widths
        columnWidths = calculateColumnWidths(tableWidth: containerWidth)
        
        // 2. Calculate actual total width needed for table
        let totalBorderWidth = styleConfig.borderWidth * CGFloat(columns.count + 1)
        let actualTableWidth = columnWidths.reduce(0, +) + totalBorderWidth
        
        // 3. Calculate header row height
        let headerHeight = calculateRowHeight(texts: columns, isHeader: true)
        
        // 4. Calculate all data row heights
        var totalHeight = headerHeight
        var rowHeights: [CGFloat] = []
        for rowData in rows {
            let rowHeight = calculateRowHeight(texts: rowData, isHeader: false)
            rowHeights.append(rowHeight)
            totalHeight += rowHeight
        }
        
        // 5. Create table content container (frame layout)
        let contentView = UIView()
        contentView.backgroundColor = styleConfig.innerBorderColor  // Border base color
        contentView.frame = CGRect(x: 0, y: 0, width: actualTableWidth, height: totalHeight)
        
        // 6. Create header row
        var currentY: CGFloat = 0
        let headerRow = createHeaderRow(width: actualTableWidth, height: headerHeight)
        headerRow.frame = CGRect(x: 0, y: currentY, width: actualTableWidth, height: headerHeight)
        contentView.addSubview(headerRow)
        currentY += headerHeight
        
        // 7. Create data rows
        for (index, rowData) in rows.enumerated() {
            let rowHeight = rowHeights[index]
            let dataRow = createDataRow(rowData: rowData, rowIndex: index, width: actualTableWidth, height: rowHeight)
            dataRow.frame = CGRect(x: 0, y: currentY, width: actualTableWidth, height: rowHeight)
            contentView.addSubview(dataRow)
            currentY += rowHeight
        }
        
        // 8. Add views based on whether horizontal scrolling is needed
        if styleConfig.enableHorizontalScroll && actualTableWidth > containerWidth {
            // Horizontal scrolling enabled and table width exceeds container width
            let scroll = UIScrollView()
            scroll.backgroundColor = .clear
            scroll.contentSize = CGSize(width: actualTableWidth, height: totalHeight)
            scroll.showsHorizontalScrollIndicator = false
            scroll.showsVerticalScrollIndicator = false
            scroll.bounces = true
            
            scroll.addSubview(contentView)
            self.addSubview(scroll)
            scroll.frame = CGRect(x: 0, y: 0, width: containerWidth, height: totalHeight)
            
            self.scrollView = scroll
        } else {
            // No horizontal scrolling needed
            self.addSubview(contentView)
            contentView.frame = CGRect(x: 0, y: 0, width: containerWidth, height: totalHeight)
        }
        
        self.tableContentView = contentView
        
        #if DEBUG
        Logger.shared.debug("Table built: \(columns.count) columns, \(rows.count) rows")
        Logger.shared.debug("Actual table width: \(actualTableWidth), height: \(totalHeight)")
        Logger.shared.debug("Container width: \(containerWidth)")
        Logger.shared.debug("Horizontal scroll enabled: \(styleConfig.enableHorizontalScroll)")
        Logger.shared.debug("Column widths: \(columnWidths)")
        #endif
    }
    
    /// Calculate column width (content-adaptive, max 600px)
    private func calculateColumnWidths(tableWidth: CGFloat) -> [CGFloat] {
        guard !columns.isEmpty else { return [] }
        
        // 1. Calculate total width occupied by borders
        let totalBorderWidth = styleConfig.borderWidth * CGFloat(columns.count + 1)
        let availableWidth = tableWidth - totalBorderWidth
        
        // 2. If column weight configured, distribute by weight
        if !styleConfig.columnWeights.isEmpty && styleConfig.columnWeights.count == columns.count {
            let totalWeight = styleConfig.columnWeights.reduce(0, +)
            return styleConfig.columnWeights.map { weight in
                availableWidth * (weight / totalWeight)
            }
        }
        
        // 3. Calculate ideal width per column based on content (using configured min/max)
        var idealWidths: [CGFloat] = []
        
        for columnIndex in 0..<columns.count {
            var maxWidth: CGFloat = styleConfig.minColumnWidth
            
            // Calculate header text width (using horizontal padding)
            let headerText = columns[columnIndex]
            let headerWidth = calculateTextWidth(text: headerText, font: headerFont) + styleConfig.cellPaddingHorizontal * 2
            maxWidth = max(maxWidth, headerWidth)
            
            // Calculate text width for all data cells in column (using horizontal padding)
            for rowData in rows {
                guard columnIndex < rowData.count else { continue }
                let cellText = rowData[columnIndex]
                let cellWidth = calculateTextWidth(text: cellText, font: cellFont) + styleConfig.cellPaddingHorizontal * 2
                maxWidth = max(maxWidth, cellWidth)
            }
            
            // Limit to configured max width
            maxWidth = min(maxWidth, styleConfig.maxColumnWidth)
            idealWidths.append(maxWidth)
        }
        
        // 4. Calculate ideal total width
        let idealTotalWidth = idealWidths.reduce(0, +)
        
        // 5. Decide column width strategy based on horizontal scrolling
        if idealTotalWidth < availableWidth {
            // Ideal total width less than available: evenly distribute available width
            let columnWidth = availableWidth / CGFloat(columns.count)
            return Array(repeating: columnWidth, count: columns.count)
        } else if styleConfig.enableHorizontalScroll {
            // Ideal total width greater than available and horizontal scroll enabled: use ideal width
            return idealWidths
        } else {
            // Ideal total width greater than available and no horizontal scroll: compress proportionally
            let scale = availableWidth / idealTotalWidth
            let scaledWidths = idealWidths.map { $0 * scale }
            return scaledWidths
        }
    }
    
    /// Calculate text width
    private func calculateTextWidth(text: String, font: UIFont) -> CGFloat {
        let attributes: [NSAttributedString.Key: Any] = [.font: font]
        let size = (text as NSString).size(withAttributes: attributes)
        return ceil(size.width)
    }
    
    /// Calculate row height (supports multi-line text) - using specified column widths array
    private func calculateRowHeight(texts: [String], columnWidths: [CGFloat], isHeader: Bool) -> CGFloat {
        let font = isHeader ? headerFont : cellFont
        var maxHeight: CGFloat = minRowHeight

        for (index, text) in texts.enumerated() {
            guard index < columnWidths.count else { break }

            let columnWidth = columnWidths[index]
            let textWidth = columnWidth - styleConfig.cellPaddingHorizontal * 2

            guard textWidth > 0 else { continue }

            let attributes: [NSAttributedString.Key: Any] = [.font: font]
            let boundingRect = (text as NSString).boundingRect(
                with: CGSize(width: textWidth, height: .greatestFiniteMagnitude),
                options: [.usesLineFragmentOrigin, .usesFontLeading],
                attributes: attributes,
                context: nil
            )

            let textHeight = ceil(boundingRect.height) + styleConfig.cellPaddingVertical * 2
            maxHeight = max(maxHeight, textHeight)
        }

        return maxHeight
    }

    /// Calculate row height (supports multi-line text) - using instance columnWidths
    private func calculateRowHeight(texts: [String], isHeader: Bool) -> CGFloat {
        return calculateRowHeight(texts: texts, columnWidths: columnWidths, isHeader: isHeader)
    }
    
    /// Create header row (using frame layout)
    private func createHeaderRow(width: CGFloat, height: CGFloat) -> UIView {
        let rowContainer = UIView()
        rowContainer.backgroundColor = styleConfig.headerBackgroundColor
        rowContainer.frame = CGRect(x: 0, y: 0, width: width, height: height)
        
        var currentX: CGFloat = 0
        
        // Left border
        let leftBorder = UIView()
        leftBorder.backgroundColor = styleConfig.innerBorderColor
        leftBorder.frame = CGRect(x: currentX, y: 0, width: styleConfig.borderWidth, height: height)
        rowContainer.addSubview(leftBorder)
        currentX += styleConfig.borderWidth
        
        // Column cells
        for (index, headerText) in columns.enumerated() {
            let columnWidth = columnWidths[index]
            let cell = createCell(text: headerText, isHeader: true, columnWidth: columnWidth, rowHeight: height)
            cell.frame = CGRect(x: currentX, y: 0, width: columnWidth, height: height)
            rowContainer.addSubview(cell)
            currentX += columnWidth
            
            // Right border / separator
            let separator = UIView()
            separator.backgroundColor = styleConfig.innerBorderColor
            separator.frame = CGRect(x: currentX, y: 0, width: styleConfig.borderWidth, height: height)
            rowContainer.addSubview(separator)
            currentX += styleConfig.borderWidth
        }
        
        return rowContainer
    }
    
    /// Create data row (using frame layout)
    private func createDataRow(rowData: [String], rowIndex: Int, width: CGFloat, height: CGFloat) -> UIView {
        let rowContainer = UIView()
        
        // Apply alternating background color
        if !styleConfig.bodyBgColors.isEmpty {
            let colorIndex = rowIndex % styleConfig.bodyBgColors.count
            rowContainer.backgroundColor = styleConfig.bodyBgColors[colorIndex]
        } else {
            rowContainer.backgroundColor = .white
        }
        rowContainer.frame = CGRect(x: 0, y: 0, width: width, height: height)
        
        var currentX: CGFloat = 0
        
        // Left border
        let leftBorder = UIView()
        leftBorder.backgroundColor = styleConfig.innerBorderColor
        leftBorder.frame = CGRect(x: currentX, y: 0, width: styleConfig.borderWidth, height: height)
        rowContainer.addSubview(leftBorder)
        currentX += styleConfig.borderWidth
        
        // Column cells
        for (index, cellText) in rowData.enumerated() {
            guard index < columnWidths.count else { break }
            
            let columnWidth = columnWidths[index]
            let cell = createCell(text: cellText, isHeader: false, columnWidth: columnWidth, rowHeight: height)
            cell.frame = CGRect(x: currentX, y: 0, width: columnWidth, height: height)
            rowContainer.addSubview(cell)
            currentX += columnWidth
            
            // Right border / separator
            let separator = UIView()
            separator.backgroundColor = styleConfig.innerBorderColor
            separator.frame = CGRect(x: currentX, y: 0, width: styleConfig.borderWidth, height: height)
            rowContainer.addSubview(separator)
            currentX += styleConfig.borderWidth
        }
        
        return rowContainer
    }
    
    /// Create cell (using frame layout)
    private func createCell(text: String, isHeader: Bool, columnWidth: CGFloat, rowHeight: CGFloat) -> UIView {
        let cellContainer = UIView()
        cellContainer.clipsToBounds = false
        
        let label = UILabel()
        label.text = text
        label.numberOfLines = 0
        label.lineBreakMode = .byCharWrapping
        
        // Font style
        let fontSize = isHeader ? styleConfig.headerFontSize : styleConfig.bodyFontSize
        let fontWeight: UIFont.Weight = ComponentStyleConfigManager.parseFontWeight(isHeader ? styleConfig.headerFontWeight : styleConfig.bodyFontWeight)
        label.font = UIFont.systemFont(ofSize: fontSize, weight: fontWeight)
        label.textColor = isHeader ? styleConfig.headerFontColor : styleConfig.bodyFontColor
        label.textAlignment = styleConfig.configTextAlign
        
        // Use frame layout, add padding
        let labelX = styleConfig.cellPaddingHorizontal
        let labelY = styleConfig.cellPaddingVertical
        let labelWidth = columnWidth - styleConfig.cellPaddingHorizontal * 2
        let labelHeight = rowHeight - styleConfig.cellPaddingVertical * 2
        label.frame = CGRect(x: labelX, y: labelY, width: labelWidth, height: labelHeight)
        
        cellContainer.addSubview(label)
        
        return cellContainer
    }
}

/// TableComponent component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - columns: Header column name array (Array<String>, required)
/// - rows: Table data row array (Array<Array<String>>, required)
///
/// Style configuration (from localConfig.json):
/// - border-width: Border width (String, default 1.0)
/// - border-radius: Border corner radius (String, default 0)
/// - border-color: Border color (String, default gray)
/// - inner-border-color: Inner cell border color (String, default light blue-gray)
/// - cell-padding: Uniform cell padding (String, default 16.0)
/// - cell-padding-vertical: Vertical cell padding (String)
/// - cell-padding-horizontal: Horizontal cell padding (String)
/// - min-column-width: Minimum column width (String, default 60.0)
/// - max-column-width: Maximum column width (String, default 600.0)
/// - horizontal-scroll: Enable horizontal scrolling (Bool, default true)
/// - text-align: Text alignment (String: left/center/right, default left)
/// - vertical-align: Vertical alignment (String, default center)
/// - header-bg-color: Header background color (String, default light gray)
/// - header-font-color: Header font color (String, default black)
/// - header-font-size: Header font size (String, default 14.0)
/// - header-font-weight: Header font weight (String, default bold)
/// - body-bg-color: Alternating row background colors (Array<String>, default white/zebra)
/// - body-font-color: Body font color (String, default black)
/// - body-font-size: Body font size (String, default 14.0)
/// - body-font-weight: Body font weight (String, default normal)
///
/// Design notes:
/// - Column width priority: column-weights > content-adaptive (constrained by min/max)
/// - When total width exceeds container: auto horizontal scroll if enabled
/// - When total width is less than container: columns evenly distribute available width
/// - Supports multi-line text auto-wrap
/// - Uses InnerTableView with frame layout for table rendering
class TableComponent: Component {
    
    // MARK: - Properties
    
    /// Inner table view
    private var innerTableView: InnerTableView?
    
    /// Style configuration
    private var styleConfig: TableStyleConfig = TableStyleConfig()
    
    // MARK: - Initialization

    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "Table", properties: properties)
        
        // Load local style configuration
        loadLocalStyleConfig()
        
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

        let columns = json["columns"] as? [String] ?? []
        let rows    = json["rows"] as? [[String]] ?? []
        guard !columns.isEmpty else { return .zero }

        // measure is called on a background thread — must NOT create any UIView.
        // All calculations use NSString text-measurement APIs which are thread-safe.
        let constraintWidth: CGFloat = (widthMode == .undefined || maxWidth <= 0)
            ? 375   // fallback when Yoga provides no width constraint
            : CGFloat(maxWidth)

        let measuredHeight = calculateTableHeight(
            columns: columns, rows: rows,
            tableWidth: constraintWidth
        )

        var measuredWidth  = constraintWidth
        var finalHeight    = measuredHeight

        if (widthMode == .exactly || widthMode == .atMost) && maxWidth > 0 {
            measuredWidth = widthMode == .atMost
                ? min(measuredWidth, CGFloat(maxWidth))
                : CGFloat(maxWidth)
        }
        if (heightMode == .exactly || heightMode == .atMost) && maxHeight > 0 {
            finalHeight = heightMode == .atMost
                ? min(finalHeight, CGFloat(maxHeight))
                : CGFloat(maxHeight)
        }

        return CGSize(width: measuredWidth, height: finalHeight)
    }

    // MARK: - Pure layout calculation (thread-safe, no UIView)

    /// Calculate total table height without creating any views.
    /// Safe to call from a background thread (used by the static measure method).
    private static func calculateTableHeight(columns: [String], rows: [[String]], tableWidth: CGFloat) -> CGFloat {
        let config = TableStyleConfig()
        let minRowHeight: CGFloat = 44.0
        let borderWidth = config.borderWidth
        let paddingH = config.cellPaddingHorizontal
        let paddingV = config.cellPaddingVertical
        let minColW  = config.minColumnWidth
        let maxColW  = config.maxColumnWidth
        let enableHScroll = config.enableHorizontalScroll

        let headerFontSize: CGFloat = config.headerFontSize
        let headerFontWeight: UIFont.Weight = ComponentStyleConfigManager.parseFontWeight(config.headerFontWeight)
        let headerFont = UIFont.systemFont(ofSize: headerFontSize, weight: headerFontWeight)

        let cellFontSize: CGFloat = config.bodyFontSize
        let cellFontWeight: UIFont.Weight = ComponentStyleConfigManager.parseFontWeight(config.bodyFontWeight)
        let cellFont = UIFont.systemFont(ofSize: cellFontSize, weight: cellFontWeight)

        // --- Column widths ---
        let totalBorderWidth = borderWidth * CGFloat(columns.count + 1)
        let availableWidth   = tableWidth - totalBorderWidth

        var idealWidths: [CGFloat] = columns.indices.map { colIdx in
            var best = minColW
            // header
            let hw = textWidth(columns[colIdx], font: headerFont) + paddingH * 2
            best = max(best, hw)
            // rows
            for row in rows {
                guard colIdx < row.count else { continue }
                let cw = textWidth(row[colIdx], font: cellFont) + paddingH * 2
                best = max(best, cw)
            }
            return min(best, maxColW)
        }

        let idealTotal = idealWidths.reduce(0, +)
        let columnWidths: [CGFloat]
        if idealTotal < availableWidth {
            let even = availableWidth / CGFloat(columns.count)
            columnWidths = Array(repeating: even, count: columns.count)
        } else if enableHScroll {
            columnWidths = idealWidths
        } else {
            let scale = availableWidth / idealTotal
            columnWidths = idealWidths.map { $0 * scale }
        }

        // --- Row heights ---
        func rowHeight(_ texts: [String], font: UIFont) -> CGFloat {
            var h = minRowHeight
            for (i, text) in texts.enumerated() {
                guard i < columnWidths.count else { break }
                let tw = columnWidths[i] - paddingH * 2
                guard tw > 0 else { continue }
                let rect = (text as NSString).boundingRect(
                    with: CGSize(width: tw, height: .greatestFiniteMagnitude),
                    options: [.usesLineFragmentOrigin, .usesFontLeading],
                    attributes: [.font: font], context: nil)
                h = max(h, ceil(rect.height) + paddingV * 2)
            }
            return h
        }

        var total = rowHeight(columns, font: headerFont)
        for row in rows { total += rowHeight(row, font: cellFont) }
        return total
    }

    /// Thread-safe single-line text width measurement.
    private static func textWidth(_ text: String, font: UIFont) -> CGFloat {
        ceil((text as NSString).size(withAttributes: [.font: font]).width)
    }

    // MARK: - Layout

    override func layoutSubviews() {
        super.layoutSubviews()

        guard let innerTableView = innerTableView, bounds.width > 0 else { return }

        // Setting frame triggers InnerTableView.frame.didSet, which calls buildTableContent
        // when the width changes or needsRebuild is true.
        if innerTableView.frame != bounds {
            innerTableView.frame = bounds
        }
    }

    override func updateProperties(_ properties: [String: Any]) {
        // Call parent method to apply CSS properties to self
        super.updateProperties(properties)
        
        let columnsArray = properties["columns"] as? [String] ?? []
        let rowsArray    = properties["rows"]    as? [[String]] ?? []
        
        if innerTableView == nil {
            let tableView = InnerTableView(frame: .zero)
            addSubview(tableView)
            innerTableView = tableView
        }
        
        if ((columnsArray.count != 0) && (rowsArray.count != 0)) {
            // reloadData sets data and immediately builds if width is already known
            innerTableView?.reloadData(columns: columnsArray, rows: rowsArray, styleConfig: styleConfig)
        }
    }
    
    override func setBorderRadius(_ radius: CGFloat) {
        super.setBorderRadius(radius)
        // Mirror corner radius to innerTableView so inner subviews (header/rows with background
        // colors) are clipped to rounded corners. clipsToBounds is managed by innerTableView.borderRadius didSet.
        innerTableView?.borderRadius = radius
    }
    
    // MARK: - Configuration Methods
    
    /// Load local style configuration
    private func loadLocalStyleConfig() {
        guard let config = ComponentStyleConfigManager.shared.getConfig(for: componentType) else {
            Logger.shared.debug("Using default configuration")
            return
        }
        
        Logger.shared.info("Loading local style configuration: \(config)")
        
        // Parse border width
        if let width = config["border-width"] as? String,
           let value = ComponentStyleConfigManager.parseSize(width) {
            styleConfig.borderWidth = value
        }
        
        // Parse border corner radius
        if let radius = config["border-radius"] as? String,
           let value = ComponentStyleConfigManager.parseSize(radius) {
            styleConfig.borderRadius = value
        }
        
        // Parse border color
        if let color = config["border-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            styleConfig.borderColor = value
        }
        
        // Parse cell padding
        if let padding = config["cell-padding"] as? String,
           let value = ComponentStyleConfigManager.parseSize(padding) {
            styleConfig.cellPaddingVertical = value
            styleConfig.cellPaddingHorizontal = value
            Logger.shared.debug("[Table] cell-padding loaded: \(value)")
        }
        
        if let padding = config["cell-padding-vertical"] as? String,
           let value = ComponentStyleConfigManager.parseSize(padding) {
            styleConfig.cellPaddingVertical = value
            Logger.shared.debug("[Table] cell-padding-vertical loaded: \(value)")
        }
        
        if let padding = config["cell-padding-horizontal"] as? String,
           let value = ComponentStyleConfigManager.parseSize(padding) {
            styleConfig.cellPaddingHorizontal = value
            Logger.shared.debug("[Table] cell-padding-horizontal loaded: \(value)")
        }
        
        // Parse column width limits
        if let minWidth = config["min-column-width"] as? String,
           let value = ComponentStyleConfigManager.parseSize(minWidth) {
            styleConfig.minColumnWidth = value
        }
        
        if let maxWidth = config["max-column-width"] as? String,
           let value = ComponentStyleConfigManager.parseSize(maxWidth) {
            styleConfig.maxColumnWidth = value
        }
        
        // Parse horizontal scroll
        if let scroll = config["horizontal-scroll"] as? Bool {
            styleConfig.enableHorizontalScroll = scroll
        }
        
        // Parse inner border color
        if let color = config["inner-border-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            styleConfig.innerBorderColor = value
        }
        
        // Parse text alignment
        if let align = config["text-align"] as? String {
            switch align.lowercased() {
            case "left":
                styleConfig.configTextAlign = .left
            case "center":
                styleConfig.configTextAlign = .center
            case "right":
                styleConfig.configTextAlign = .right
            default:
                styleConfig.configTextAlign = .left
            }
        }
        
        // Parse vertical alignment
        if let vAlign = config["vertical-align"] as? String {
            styleConfig.configVerticalAlign = vAlign
        }
        
        // Parse header style
        if let color = config["header-bg-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            styleConfig.headerBackgroundColor = value
        }
        
        if let color = config["header-font-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            styleConfig.headerFontColor = value
        }
        
        if let size = config["header-font-size"] as? String,
           let value = ComponentStyleConfigManager.parseSize(size) {
            styleConfig.headerFontSize = value
        }
        
        if let weight = config["header-font-weight"] as? String {
            styleConfig.headerFontWeight = weight
        }
        
        // Parse body style
        if let bgColors = config["body-bg-color"] as? [String] {
            styleConfig.bodyBgColors = bgColors.compactMap { ComponentStyleConfigManager.parseColorToUIColor($0) }
        }
        
        if let color = config["body-font-color"] as? String,
           let value = ComponentStyleConfigManager.parseColorToUIColor(color) {
            styleConfig.bodyFontColor = value
        }
        
        if let size = config["body-font-size"] as? String,
           let value = ComponentStyleConfigManager.parseSize(size) {
            styleConfig.bodyFontSize = value
        }
        
        if let weight = config["body-font-weight"] as? String {
            styleConfig.bodyFontWeight = weight
        }
    }
}

#endif // AGENUI_SDK_BUILD