#pragma once

#include "agenui_data_value_base.h"
#include <memory>
#include <vector>

namespace agenui {

class IDataChangedObserver;

/**
 * @brief Interpolation expression data value
 * @remark Composite data value representing an expression composed of multiple segments.
 *         Each segment is itself a DataValue and can be either a static string
 *         (e.g. StaticDataValue), a data-model binding (e.g. DataBindingDataValue),
 *         or a client-side function call (e.g. FunctionCallDataValue). This class
 *         aggregates binding status, value data, bind/unbind lifecycle, and
 *         template cloning across all segments.
 */
class InterpolationExpressionDataValue : public DataValue {
public:
    InterpolationExpressionDataValue(IDataValueContext* context,
                                     const std::vector<std::shared_ptr<DataValue>>& segments);
    ~InterpolationExpressionDataValue() override;

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const override;

    /**
     * @brief Get all segments that make up this expression
     * @return Ordered list of DataValue segments (static string / interpolation / ...)
     */
    std::vector<std::shared_ptr<DataValue>> getSegments() const;

private:
    std::vector<std::shared_ptr<DataValue>> _segments;
};

}  // namespace agenui
