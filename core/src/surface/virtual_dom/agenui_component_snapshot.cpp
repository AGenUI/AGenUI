#include "agenui_component_snapshot.h"
#include "surface/yoga_node/agenui_css_style_converter.h"
#include "surface/agenui_serializable_data_impl.h"

namespace agenui {

bool LayoutInfo::operator==(const LayoutInfo& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
}

bool LayoutInfo::operator!=(const LayoutInfo& other) const {
    return !(*this == other);
}

std::string ComponentSnapshot::stringify() const {
    auto dataImpl = SerializableData::Impl::createObject();
    
    for (const auto& attr : attributes) {
        if (attr.second.isNull() || !attr.second.isValid()) {
            continue;
        }
        
        // Streaming append mode: use incremental field names
        if (component == "Markdown" && attr.first == "content" && appendMode) {
            dataImpl->set("appendContent", attr.second);
        } else if (component == "Text" && attr.first == "text" && appendMode) {
            // Emit text under both keys: "text" for measurement, "textChunk" for incremental append
            dataImpl->set("text", attr.second);
            dataImpl->set("textChunk", attr.second);
        } else {
            dataImpl->set(attr.first, attr.second);
        }
    }
    
    dataImpl->set("id", id);
    dataImpl->set("component", component);
    
    if (!styles.empty() || layout.isValid()) {
        auto stylesImpl = SerializableData::Impl::createObject();

        for (const auto& style : styles) {
            if (style.second.isNull() || !style.second.isValid()) {
                continue;
            }
            stylesImpl->set(style.first, style.second);
        }
        // Include Yoga layout results in styles
        stylesImpl->set("x", layout.x);
        stylesImpl->set("y", layout.y);
        stylesImpl->set("width", layout.width);
        stylesImpl->set("height", layout.height);
        // Output line count for text components when layout dimensions are set
        if ((component == "Text" || component == "RichText") && layout.lines > 0) {
            stylesImpl->set("lines", layout.lines);
        }
        // Include styleInfo (saved original width/height style)
        if (!layout.styleInfo.empty()) {
            stylesImpl->set("styleInfo", layout.styleInfo);
        }
        dataImpl->set("styles", SerializableData(stylesImpl));
    }

    if (!children.empty()) {
        auto childrenImpl = SerializableData::Impl::createArray();
        for (const auto& child : children) {
            childrenImpl->append(SerializableData(SerializableData::Impl::create(child)));
        }
        dataImpl->set("children", SerializableData(childrenImpl));
    }

    return SerializableData(dataImpl).dump();
}

void ComponentSnapshot::resetMode() {
    appendMode = false;
}

}  // namespace agenui
