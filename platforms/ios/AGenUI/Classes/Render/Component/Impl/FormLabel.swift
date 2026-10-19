#if AGENUI_SDK_BUILD
import UIKit

/// Shared title metrics for form controls; empty labels occupy no space.
final class FormLabel: UILabel {
    static let spacing: CGFloat = 16 * Component.BS_POINT_SCALE
    static let labelFont = UIFont.systemFont(ofSize: 32 * Component.BS_POINT_SCALE)

    init() {
        super.init(frame: .zero)
        font = Self.labelFont
        textColor = .black
        numberOfLines = 0
        isHidden = true
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    static func text(from value: Any?) -> String {
        if let text = value as? String { return text }
        return (value as? [String: Any])?["literalString"] as? String ?? ""
    }

    static func measure(_ value: Any?, width: CGFloat) -> CGSize {
        let text = text(from: value)
        guard !text.isEmpty else { return .zero }
        let rect = (text as NSString).boundingRect(
            with: CGSize(width: max(1, width), height: .greatestFiniteMagnitude),
            options: [.usesLineFragmentOrigin, .usesFontLeading],
            attributes: [.font: labelFont], context: nil)
        return CGSize(width: ceil(rect.width), height: ceil(rect.height) + spacing)
    }

    func update(_ diff: DiffValue?) {
        switch diff {
        case .value(let value): text = Self.text(from: value)
        case .deleted: text = nil
        default: return
        }
        isHidden = text?.isEmpty ?? true
    }

    @discardableResult
    func layout(width: CGFloat) -> CGFloat {
        let size = Self.measure(text, width: width)
        frame = CGRect(x: 0, y: 0, width: width, height: max(0, size.height - Self.spacing))
        return size.height
    }
}
#endif
