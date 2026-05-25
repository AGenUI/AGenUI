#pragma once

#include "agenui_measurement.h"
#include "text_component_measurement.h"
#include "nlohmann/json.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace a2ui {

/**
 * @brief Table component measurement class
 *
 * Does not depend on Yoga; directly within measure():
 *  1. Parse columns/rows/cellPadding/columnWeights
 *  2. Measure each cell text height (including font weight) using TextMeasurementUtils::measureTextHeight
 *  3. Sum row heights to get total height
 */
class TableComponentMeasurement : public agenui::IMeasurement {
public:
    TableComponentMeasurement() = default;
    ~TableComponentMeasurement() = default;

    // IMeasurement
    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;

private:
    struct ParsedTable {
        std::vector<std::string>                headers;
        std::vector<std::vector<std::string>>   rows;
        std::vector<float>                      columnWeights;
        float                                   cellPadding = -1.0f;  // <0 means not explicitly set; use configuration default
    };

    ParsedTable parseTable(const std::string& paramJson) const;
    std::vector<float> calcColumnWidths(int colCount, float maxWidth,
                                        const std::vector<float>& weights) const;
    float measureCellText(const std::string& text, float colWidth,
                          float cellPaddingH, float cellPaddingV,
                          nlohmann::json& cellStyleJson) const;

};

}  // namespace a2ui
