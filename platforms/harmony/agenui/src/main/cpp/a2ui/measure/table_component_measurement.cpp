#include "table_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "text_component_measurement.h"
#include "hm_text_measure_utils.h"
#include "a2ui/third_party/key_define.h"

#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstdlib>
#include <climits>

namespace a2ui {

// ==================== IMeasurement::measure ====================

agenui::MeasureResult TableComponentMeasurement::measure(
        const std::string& paramJson,
        const agenui::MeasureModes& modes) {

    ParsedTable table = parseTable(paramJson);

    float maxWidth = modes.width.maxValue > 0.0f ? modes.width.maxValue : 720.0f;
    int colCount = static_cast<int>(table.headers.size());
    if (colCount == 0 && !table.rows.empty()) {
        colCount = static_cast<int>(table.rows[0].size());
    }
    if (colCount == 0) {
        return {agenui::CalcType::Sync, maxWidth, 0.0f, 0};
    }

    std::vector<float> colWidths = calcColumnWidths(colCount, maxWidth, table.columnWeights);

    // Read style defaults from g_component_styles
    float cellPaddingH = 32.0f;
    float cellPaddingV = 20.0f;
    // Complete style JSON for header/body (for buildParam to parse font-size, font-weight, etc.)
    nlohmann::json headerStyleJson;
    nlohmann::json bodyStyleJson;

    const nlohmann::json& tblStyles = ::a2ui::getComponentStylesFor("Table");
    if (tblStyles.is_object()) {
        parseStyleFloat(tblStyles, "cell-padding-horizontal", cellPaddingH);
        parseStyleFloat(tblStyles, "cell-padding-vertical",   cellPaddingV);

        // Build header/body text style JSON (flat format, consistent with stringify())
        // buildParam reads font-size, font-weight, etc. from root level
        auto buildCellStyleJson = [&tblStyles](const char* fontSizeKey,
                                               const char* fontWeightKey) -> nlohmann::json {
            nlohmann::json j = nlohmann::json::object();
            if (tblStyles.contains(fontSizeKey))   j["font-size"]   = tblStyles[fontSizeKey];
            if (tblStyles.contains(fontWeightKey)) j["font-weight"] = tblStyles[fontWeightKey];
            j["text"] = "";
            return j;
        };
        headerStyleJson = buildCellStyleJson("header-font-size", "header-font-weight");
        bodyStyleJson   = buildCellStyleJson("body-font-size",   "body-font-weight");
    }

    // Explicitly specified cell-padding in paramJson takes higher priority
    if (table.cellPadding >= 0.0f) {
        cellPaddingH = table.cellPadding;
        cellPaddingV = table.cellPadding;
    }

    float totalHeight = 0.0f;

    // Calculate header row height
    if (!table.headers.empty()) {
        float rowHeight = 0.0f;
        for (int c = 0; c < colCount; c++) {
            const std::string& text = (c < (int)table.headers.size()) ? table.headers[c] : "";
            float h = measureCellText(text, colWidths[c], cellPaddingH, cellPaddingV, headerStyleJson);
            rowHeight = std::max(rowHeight, h);
        }
        totalHeight += rowHeight;
    }

    // Calculate data rows
    for (const auto& row : table.rows) {
        float rowHeight = 0.0f;
        for (int c = 0; c < colCount; c++) {
            const std::string& text = (c < (int)row.size()) ? row[c] : "";
            float h = measureCellText(text, colWidths[c], cellPaddingH, cellPaddingV, bodyStyleJson);
            rowHeight = std::max(rowHeight, h);
        }
        totalHeight += rowHeight;
    }

    return {agenui::CalcType::Sync, maxWidth, totalHeight, 0};
}

// ==================== Parsing ====================

TableComponentMeasurement::ParsedTable TableComponentMeasurement::parseTable(
        const std::string& paramJson) const {
    ParsedTable result;
    nlohmann::json j = nlohmann::json::parse(paramJson, nullptr, false);
    if (j.is_discarded()) return result;

    // columns (header)
    // stringify() flattens attributes to root level without "attrs" wrapper
    if (j.contains("columns")) {
        const auto& cols = j["columns"];
        if (cols.is_array()) {
            for (const auto& col : cols) {
                result.headers.push_back(col.is_string() ? col.get<std::string>() : col.dump());
            }
        }
    }

    // rows (data rows)
    if (j.contains("rows")) {
        const auto& rows = j["rows"];
        if (rows.is_array()) {
            for (const auto& rowJson : rows) {
                if (!rowJson.is_array()) continue;
                std::vector<std::string> row;
                for (const auto& cell : rowJson) {
                    row.push_back(cell.is_string() ? cell.get<std::string>() : cell.dump());
                }
                result.rows.push_back(std::move(row));
            }
        }
    }

    // columnWeights
    if (j.contains("styles") && j["styles"].contains("column-weights")) {
        const auto& cw = j["styles"]["column-weights"];
        if (cw.is_array()) {
            for (const auto& w : cw) {
                if (w.is_number()) {
                    result.columnWeights.push_back(w.get<float>());
                } else {
                    const std::string ws = w.is_string() ? w.get<std::string>() : w.dump();
                    if (!ws.empty()) {
                        result.columnWeights.push_back(static_cast<float>(std::atof(ws.c_str())));
                    } else {
                        result.columnWeights.push_back(1.0f);
                    }
                }
            }
        }
    }

    // cellPadding
    if (j.contains("styles") && j["styles"].contains("cell-padding")) {
        const auto& cp = j["styles"]["cell-padding"];
        if (cp.is_number()) result.cellPadding = cp.get<float>();
    }

    return result;
}

// ==================== Column Width Calculation ====================

std::vector<float> TableComponentMeasurement::calcColumnWidths(
        int colCount, float maxWidth, const std::vector<float>& weights) const {
    float totalWeight = 0.0f;
    for (int i = 0; i < colCount; i++) {
        totalWeight += (i < (int)weights.size()) ? weights[i] : 1.0f;
    }
    std::vector<float> widths(colCount);
    for (int i = 0; i < colCount; i++) {
        float w = (i < (int)weights.size()) ? weights[i] : 1.0f;
        widths[i] = (totalWeight > 0.0f) ? (maxWidth * w / totalWeight) : (maxWidth / colCount);
    }
    return widths;
}

// ==================== Cell Text Measurement ====================
// Reuse TextComponentMeasurement::buildParam, sharing the same font parsing logic as Text component

float TableComponentMeasurement::measureCellText(
        const std::string& text, float colWidth,
        float cellPaddingH, float cellPaddingV,
        nlohmann::json& cellStyleJson) const {
    float innerWidth = colWidth - cellPaddingH * 2.0f;
    if (innerWidth < 1.0f) innerWidth = 1.0f;

    // Inject text in-place to avoid copying the entire json object
    cellStyleJson["text"] = text;

    // Use TextComponentMeasurement::buildParam to parse complete font parameters
    std::string outText;
    TextMeasureParam param;
    if (!TextComponentMeasurement::buildParam(cellStyleJson, outText, param)) {
        // Empty text: return height with padding only
        return cellPaddingV * 2.0f;
    }

    float baseLine = 0.f, ascent = 0.f, descent = 0.f;
    auto r = TextMeasureUtils::doMeasure(
        param,
        innerWidth, MeasureModeAtMost,
        0.0f,       MeasureModeUndefined,
        baseLine, ascent, descent);

    return r.height + cellPaddingV * 2.0f;
}

}  // namespace a2ui
