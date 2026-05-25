#include "agenui_interpolation_expression_data_value.h"
#include "surface/datamodel/agenui_data_observer.h"
#include "surface/agenui_serializable_data_impl.h"

namespace agenui {

InterpolationExpressionDataValue::InterpolationExpressionDataValue(
    IDataValueContext* context,
    const std::vector<std::shared_ptr<DataValue>>& segments)
    : DataValue(context), _segments(segments) {
}

InterpolationExpressionDataValue::~InterpolationExpressionDataValue() {
    unbind();
}

DataType InterpolationExpressionDataValue::getDataType() const {
    return DataType::InterpolationExpressionData;
}

DataBindingStatus InterpolationExpressionDataValue::getDataBindingStatus() const {
    if (_segments.empty()) {
        return DataBindingStatus::NotDependent;
    }

    std::vector<DataBindingStatus> statuses;
    statuses.reserve(_segments.size());
    for (const auto& segment : _segments) {
        if (!segment) {
            continue;
        }
        statuses.emplace_back(segment->getDataBindingStatus());
    }

    return DataValue::aggregateBindingStatus(statuses);
}

SerializableData InterpolationExpressionDataValue::getValueData() const {
    // Render each segment as a string fragment and concatenate them in order.
    // String-typed segments are taken verbatim (no surrounding JSON quotes);
    // other JSON types (number / bool / null / array / object) fall back to
    // their JSON dump representation. Invalid segments are skipped silently.
    std::string concatenated;
    for (const auto& segment : _segments) {
        if (!segment) {
            continue;
        }
        SerializableData segmentData = segment->getValueData();
        if (!segmentData.isValid()) {
            continue;
        }
        if (segmentData.isString()) {
            concatenated.append(segmentData.asString());
        } else {
            concatenated.append(segmentData.dump());
        }
    }
    return SerializableData(SerializableData::Impl::create(concatenated));
}

void InterpolationExpressionDataValue::bind(IDataChangedObserver* observer) {
    for (const auto& segment : _segments) {
        if (segment) {
            segment->bind(observer);
        }
    }
}

void InterpolationExpressionDataValue::unbind() {
    for (const auto& segment : _segments) {
        if (segment) {
            segment->unbind();
        }
    }
}

std::shared_ptr<DataValue> InterpolationExpressionDataValue::cloneAsTemplate(
    IDataValueContext* context, const std::string& rootDataPath) const {
    std::vector<std::shared_ptr<DataValue>> clonedSegments;
    clonedSegments.reserve(_segments.size());

    for (const auto& segment : _segments) {
        if (!segment) {
            clonedSegments.emplace_back(nullptr);
            continue;
        }
        clonedSegments.emplace_back(segment->cloneAsTemplate(context, rootDataPath));
    }

    return std::make_shared<InterpolationExpressionDataValue>(context, clonedSegments);
}

std::vector<std::shared_ptr<DataValue>> InterpolationExpressionDataValue::getSegments() const {
    return _segments;
}

}  // namespace agenui
