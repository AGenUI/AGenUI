#pragma once

#include <string>

namespace a2ui {
namespace ComponentType {

    // Base components
    inline const std::string kText          = "Text";
    inline const std::string kButton        = "Button";
    inline const std::string kImage         = "Image";
    inline const std::string kIcon          = "Icon";
    inline const std::string kDivider       = "Divider";
    inline const std::string kVideo         = "Video";
    inline const std::string kAudioPlayer   = "AudioPlayer";
    inline const std::string kModal         = "Modal";

    // Layout components
    inline const std::string kColumn        = "Column";
    inline const std::string kRow           = "Row";
    inline const std::string kCard          = "Card";
    inline const std::string kTabs          = "Tabs";
    inline const std::string kList          = "List";

    // Interactive components
    inline const std::string kTextField     = "TextField";
    inline const std::string kCheckBox      = "CheckBox";
    inline const std::string kSlider        = "Slider";
    inline const std::string kChoicePicker  = "ChoicePicker";
    inline const std::string kDateTimeInput = "DateTimeInput";

    // Extended components
    inline const std::string kRichText      = "RichText";
    inline const std::string kLottie        = "Lottie";
    inline const std::string kWeb           = "Web";
    inline const std::string kTable         = "Table";
    inline const std::string kCarousel      = "Carousel";

} // namespace ComponentType
} // namespace a2ui
