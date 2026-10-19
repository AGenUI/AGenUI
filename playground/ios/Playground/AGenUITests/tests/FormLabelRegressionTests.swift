import Testing
import UIKit
@testable import AGenUI

// Component title regression coverage for issue #146 and Aone 86819194.
@MainActor
@Suite struct FormLabelRegressionTests {
    @Test func sliderTitleUpdatesAndDeletionPreserveValue() throws {
        let slider = SliderComponent(componentId: "slider-label", properties: [
            "label": "Volume", "min": 0, "max": 100, "value": 42
        ])
        slider.frame = CGRect(x: 0, y: 0, width: 160, height: 120)
        slider.layoutIfNeeded()
        let title = try #require(slider.subviews.compactMap { $0 as? UILabel }.first)
        let control = try #require(slider.subviews.compactMap { $0 as? UISlider }.first)
        #expect(title.text == "Volume")
        #expect(!title.isHidden)
        #expect(control.frame.minY >= title.frame.maxY)
        slider.updateProperties(["label": .value("Changed volume")])
        slider.layoutIfNeeded()
        #expect(title.text == "Changed volume")
        #expect(control.value == 42)
        slider.updateProperties(["label": .deleted])
        slider.layoutIfNeeded()
        #expect(title.isHidden)
        #expect(control.frame.minY == 0)
        #expect(control.value == 42)
    }

    @Test func emptyPickerStillMeasuresWrappedTitle() throws {
        let text = "A long title that must wrap across several lines in a narrow form"
        let properties: [String: Any] = ["label": text, "options": [], "value": []]
        let data = try JSONSerialization.data(withJSONObject: properties)
        let size = ChoicePickerComponent.measure(type: "ChoicePicker",
            paramJson: String(decoding: data, as: UTF8.self), maxWidth: 90,
            widthMode: .exactly, maxHeight: 0, heightMode: .undefined)
        let picker = ChoicePickerComponent(componentId: "empty-picker-label", properties: properties)
        picker.frame = CGRect(origin: .zero, size: size)
        picker.layoutIfNeeded()
        let title = try #require(picker.subviews.compactMap { $0 as? FormLabel }.first)
        let intrinsic = title.sizeThatFits(CGSize(width: 90, height: CGFloat.greatestFiniteMagnitude))
        #expect(size.width == 90)
        #expect(size.height > title.font.lineHeight * 2)
        #expect(size.height >= intrinsic.height)
        #expect(title.frame.maxY <= size.height)
        picker.updateProperties(["label": .value("")])
        picker.layoutIfNeeded()
        #expect(title.isHidden)
        #expect(title.frame.height == 0)
    }

    @Test func sliderMeasureHonorsHeightConstraintsWithTitle() {
        let payload = #"{"label":"A multiline slider title that wraps","max":100,"value":50}"#
        let natural = SliderComponent.measure(type: "Slider", paramJson: payload,
            maxWidth: 80, widthMode: .exactly, maxHeight: 0, heightMode: .undefined)
        let bounded = SliderComponent.measure(type: "Slider", paramJson: payload,
            maxWidth: 80, widthMode: .exactly, maxHeight: 30, heightMode: .atMost)
        #expect(natural.height > 30)
        #expect(bounded.height == 30)
    }
}
