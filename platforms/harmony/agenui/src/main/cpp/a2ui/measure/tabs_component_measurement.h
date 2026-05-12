#pragma once

#include "agenui_measurement.h"
#include "text_component_measurement.h"
#include <memory>
#include <vector>
#include <string>

namespace a2ui {

/**
 * @brief Tabs component measurement class
 *
 * Does not depend on Yoga; directly within measure():
 *  1. Parse tabs array (title field)
 *  2. Measure each tab label text height using TextMeasurementUtils
 *  3. Return (maxWidth, tabRowHeight + subHeight)
 */
class TabsComponentMeasurement : public agenui::IMeasurement {
public:
    TabsComponentMeasurement() = default;
    ~TabsComponentMeasurement() = default;

    // IMeasurement
    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;

    /**
     * @brief Tabs does not set measureFunc when it has Yoga child nodes.
     *        Tabs height is determined by Yoga subtree (absolutely-positioned content area + secondary layout minHeight).
     *        Note: Yoga treats nodes with measureFunc as leaf nodes; child layouts will not be executed.
     */
    bool allowsMeasureWithChildren() const override { return false; }

private:
};

}  // namespace a2ui
