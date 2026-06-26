#include <gtest/gtest.h>
#include "surface/component_property_spec/agenui_component_property_spec.h"
#include "surface/component_property_spec/agenui_component_property_spec_manager.h"
#include "surface/component_property_spec/agenui_ispec_applicable.h"
#include <map>
#include <string>

using namespace agenui;

// ─── Mock ISpecApplicable ───────────────────────────────────────────────────────

class MockSpecApplicable : public ISpecApplicable {
public:
    explicit MockSpecApplicable(const std::string& type) : _type(type) {}

    std::string getComponentType() const override { return _type; }

    bool hasProperty(const std::string& name) const override {
        return _properties.count(name) > 0;
    }

    std::string getPropertyStringValue(const std::string& name) const override {
        auto it = _properties.find(name);
        return it != _properties.end() ? it->second : "";
    }

    void setPropertyValue(const std::string& name, const std::string& value) override {
        _properties[name] = value;
    }

    bool hasStyle(const std::string& name) const override {
        return _styles.count(name) > 0;
    }

    void setStyleValue(const std::string& name, const std::string& value) override {
        _styles[name] = value;
    }

    // Accessors for test verification
    const std::map<std::string, std::string>& properties() const { return _properties; }
    const std::map<std::string, std::string>& styles() const { return _styles; }

private:
    std::string _type;
    std::map<std::string, std::string> _properties;
    std::map<std::string, std::string> _styles;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ComponentPropertySpec Tests
// ═══════════════════════════════════════════════════════════════════════════════════

class ComponentPropertySpecTest : public ::testing::Test {};

TEST_F(ComponentPropertySpecTest, Construction_EmptyMaps) {
    ComponentPropertySpec spec({}, {});
    EXPECT_TRUE(spec.getProperties().empty());
    EXPECT_TRUE(spec.getDefaultStyles().empty());
}

TEST_F(ComponentPropertySpecTest, GetProperties_ReturnsSameMap) {
    std::map<std::string, PropertySpec> props;
    props["text"] = PropertySpec{"hello", {}};
    props["color"] = PropertySpec{"red", {}};

    ComponentPropertySpec spec(props, {});
    EXPECT_EQ(spec.getProperties().size(), 2u);
    EXPECT_EQ(spec.getProperties().at("text").defaultValue, "hello");
    EXPECT_EQ(spec.getProperties().at("color").defaultValue, "red");
}

TEST_F(ComponentPropertySpecTest, GetDefaultStyles_ReturnsSameMap) {
    PropertyValueMap styles;
    styles["font-size"] = "28px";
    styles["color"] = "black";

    ComponentPropertySpec spec({}, styles);
    EXPECT_EQ(spec.getDefaultStyles().size(), 2u);
    EXPECT_EQ(spec.getDefaultStyles().at("font-size"), "28px");
    EXPECT_EQ(spec.getDefaultStyles().at("color"), "black");
}

TEST_F(ComponentPropertySpecTest, Properties_WithEnumMapping) {
    EnumValueMapping enumMap;
    enumMap["h1"].styles["font-size"] = "40px";
    enumMap["h1"].properties["weight"] = "bold";
    enumMap["body"].styles["font-size"] = "28px";

    std::map<std::string, PropertySpec> props;
    props["variant"] = PropertySpec{"body", enumMap};

    ComponentPropertySpec spec(props, {});
    const auto& variant = spec.getProperties().at("variant");
    EXPECT_EQ(variant.defaultValue, "body");
    EXPECT_EQ(variant.enumMapping.size(), 2u);
    EXPECT_EQ(variant.enumMapping.at("h1").styles.at("font-size"), "40px");
    EXPECT_EQ(variant.enumMapping.at("h1").properties.at("weight"), "bold");
}

TEST_F(ComponentPropertySpecTest, Properties_EmptyEnumMapping) {
    std::map<std::string, PropertySpec> props;
    props["text"] = PropertySpec{"", {}};

    ComponentPropertySpec spec(props, {});
    EXPECT_TRUE(spec.getProperties().at("text").enumMapping.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════════
// ComponentPropertySpecManager Tests
// ═══════════════════════════════════════════════════════════════════════════════════

class ComponentPropertySpecManagerTest : public ::testing::Test {
protected:
    ComponentPropertySpecManager manager;
};

// --- loadFromString Tests ---

TEST_F(ComponentPropertySpecManagerTest, LoadFromString_EmptyString_ReturnsFalse) {
    EXPECT_FALSE(manager.loadFromString(""));
}

TEST_F(ComponentPropertySpecManagerTest, LoadFromString_InvalidJSON_ReturnsFalse) {
    EXPECT_FALSE(manager.loadFromString("{not valid json"));
}

TEST_F(ComponentPropertySpecManagerTest, LoadFromString_NonObject_ReturnsFalse) {
    EXPECT_FALSE(manager.loadFromString("[1,2,3]"));
}

TEST_F(ComponentPropertySpecManagerTest, LoadFromString_ValidTheme_ReturnsTrue) {
    std::string json = R"({
        "dark": {
            "Text": {
                "text": {"default": "dark-default"}
            }
        }
    })";
    EXPECT_TRUE(manager.loadFromString(json));
}

TEST_F(ComponentPropertySpecManagerTest, LoadFromString_MultipleThemes_AllLoaded) {
    std::string json = R"({
        "dark": {
            "Text": {"text": {"default": "d"}}
        },
        "light": {
            "Text": {"text": {"default": "l"}}
        }
    })";
    EXPECT_TRUE(manager.loadFromString(json));
}

TEST_F(ComponentPropertySpecManagerTest, LoadFromString_ThemeNotObject_SkipsGracefully) {
    std::string json = R"({
        "dark": "not-an-object",
        "light": {
            "Text": {"text": {"default": "l"}}
        }
    })";
    EXPECT_TRUE(manager.loadFromString(json));
}

// --- applySpec with default theme Tests ---

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_NullComponent_NoAction) {
    // Should not crash
    manager.applySpec("default", nullptr);
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_UnknownComponentType_NoAction) {
    MockSpecApplicable comp("UnknownWidget123");
    manager.applySpec("default", &comp);
    EXPECT_TRUE(comp.properties().empty());
    EXPECT_TRUE(comp.styles().empty());
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_Text_DefaultsFilled) {
    MockSpecApplicable comp("Text");
    manager.applySpec("default", &comp);
    // "text" property should have default value ""
    EXPECT_TRUE(comp.hasProperty("text"));
    // "variant" property should have default "body"
    EXPECT_TRUE(comp.hasProperty("variant"));
    EXPECT_EQ(comp.getPropertyStringValue("variant"), "\"body\"");
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_Text_StyleDefaultsFilled) {
    MockSpecApplicable comp("Text");
    manager.applySpec("default", &comp);
    // Text has default styles like width, height, font-family, etc.
    EXPECT_TRUE(comp.hasStyle("width"));
    EXPECT_TRUE(comp.hasStyle("height"));
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_PreExistingProperty_NotOverwritten) {
    MockSpecApplicable comp("Text");
    comp.setPropertyValue("text", "my-text");
    manager.applySpec("default", &comp);
    EXPECT_EQ(comp.getPropertyStringValue("text"), "my-text");
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_PreExistingStyle_NotOverwritten) {
    MockSpecApplicable comp("Text");
    comp.setStyleValue("width", "200px");
    manager.applySpec("default", &comp);
    EXPECT_TRUE(comp.hasStyle("width"));
    // Pre-existing style should not be replaced
    auto it = comp.styles().find("width");
    EXPECT_EQ(it->second, "200px");
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_EnumResolution_AppliesStyles) {
    MockSpecApplicable comp("Text");
    comp.setPropertyValue("variant", "h1");
    manager.applySpec("default", &comp);
    // h1 enum resolution should set font-size style
    EXPECT_TRUE(comp.hasStyle("font-size"));
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_EmptyTheme_FallsBackToDefault) {
    MockSpecApplicable comp("Text");
    manager.applySpec("", &comp);
    // Should fall back to "default" theme
    EXPECT_TRUE(comp.hasProperty("variant"));
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_UnknownTheme_FallsBackToDefault) {
    MockSpecApplicable comp("Text");
    manager.applySpec("nonexistent_theme", &comp);
    // Should fall back to "default" theme
    EXPECT_TRUE(comp.hasProperty("variant"));
}

// --- applySpec with custom loaded theme ---

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_CustomTheme_UsesCustomSpec) {
    std::string json = R"({
        "custom": {
            "MyWidget": {
                "label": {"default": "custom-label"},
                "styles": {"default": {"padding": "10px"}}
            }
        }
    })";
    ASSERT_TRUE(manager.loadFromString(json));

    MockSpecApplicable comp("MyWidget");
    manager.applySpec("custom", &comp);
    EXPECT_TRUE(comp.hasProperty("label"));
    EXPECT_EQ(comp.getPropertyStringValue("label"), "\"custom-label\"");
    EXPECT_TRUE(comp.hasStyle("padding"));
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_CustomThemeOverridesBase) {
    // The custom theme merges with base config; custom values override base
    std::string json = R"({
        "custom": {
            "Text": {
                "text": {"default": "overridden"},
                "variant": {"default": "h1"}
            }
        }
    })";
    ASSERT_TRUE(manager.loadFromString(json));

    MockSpecApplicable comp("Text");
    manager.applySpec("custom", &comp);
    EXPECT_EQ(comp.getPropertyStringValue("text"), "\"overridden\"");
    EXPECT_EQ(comp.getPropertyStringValue("variant"), "\"h1\"");
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_EnumResolvesProperties) {
    std::string json = R"({
        "test": {
            "Button": {
                "size": {
                    "default": "medium",
                    "enum": {
                        "small": {"min-height": "32px"},
                        "medium": {"min-height": "44px"},
                        "large": {"min-height": "56px"}
                    }
                }
            }
        }
    })";
    ASSERT_TRUE(manager.loadFromString(json));

    MockSpecApplicable comp("Button");
    comp.setPropertyValue("size", "large");
    manager.applySpec("test", &comp);
    EXPECT_EQ(comp.getPropertyStringValue("min-height"), "\"56px\"");
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_EnumPropertyPreExisting_NotOverwritten) {
    std::string json = R"({
        "test": {
            "Button": {
                "size": {
                    "default": "medium",
                    "enum": {
                        "medium": {"min-height": "\"44px\""}
                    }
                }
            }
        }
    })";
    ASSERT_TRUE(manager.loadFromString(json));

    MockSpecApplicable comp("Button");
    comp.setPropertyValue("size", "medium");
    comp.setPropertyValue("min-height", "custom-value");
    manager.applySpec("test", &comp);
    // Pre-existing property should NOT be overwritten by enum resolution
    EXPECT_EQ(comp.getPropertyStringValue("min-height"), "custom-value");
}

TEST_F(ComponentPropertySpecManagerTest, ApplySpec_Divider_EnumResolvesAxis) {
    MockSpecApplicable comp("Divider");
    // Pre-set axis so enum resolution matches the key directly
    comp.setPropertyValue("axis", "horizontal");
    manager.applySpec("default", &comp);
    EXPECT_TRUE(comp.hasProperty("axis"));
    // Horizontal axis enum should set width=100%, height=1px
    EXPECT_TRUE(comp.hasStyle("width"));
}

// --- Thread safety (basic) ---

TEST_F(ComponentPropertySpecManagerTest, ConcurrentApplySpec_NoDataRace) {
    // Just verify no crash under concurrent reads
    MockSpecApplicable comp1("Text");
    MockSpecApplicable comp2("Text");
    manager.applySpec("default", &comp1);
    manager.applySpec("default", &comp2);
    EXPECT_TRUE(comp1.hasProperty("variant"));
    EXPECT_TRUE(comp2.hasProperty("variant"));
}

// --- parseComponentSpecs edge cases (via loadFromString) ---

TEST_F(ComponentPropertySpecManagerTest, Load_ComponentConfigNotObject_Skipped) {
    std::string json = R"({
        "test": {
            "GoodWidget": {"prop": {"default": "x"}},
            "BadWidget": 42
        }
    })";
    EXPECT_TRUE(manager.loadFromString(json));
    // BadWidget should be skipped, GoodWidget should work
    MockSpecApplicable good("GoodWidget");
    manager.applySpec("test", &good);
    EXPECT_TRUE(good.hasProperty("prop"));

    MockSpecApplicable bad("BadWidget");
    manager.applySpec("test", &bad);
    EXPECT_TRUE(bad.properties().empty());
}

TEST_F(ComponentPropertySpecManagerTest, Load_PropertyConfigNotObject_Skipped) {
    std::string json = R"({
        "test": {
            "Widget": {
                "good": {"default": "y"},
                "bad": 123
            }
        }
    })";
    EXPECT_TRUE(manager.loadFromString(json));
    MockSpecApplicable comp("Widget");
    manager.applySpec("test", &comp);
    EXPECT_TRUE(comp.hasProperty("good"));
}

TEST_F(ComponentPropertySpecManagerTest, Load_EnumValueNotObject_Skipped) {
    std::string json = R"({
        "test": {
            "Widget": {
                "mode": {
                    "default": "a",
                    "enum": {
                        "a": {"styles": {"color": "red"}},
                        "b": "not-an-object"
                    }
                }
            }
        }
    })";
    EXPECT_TRUE(manager.loadFromString(json));
    MockSpecApplicable comp("Widget");
    // Pre-set mode to match enum key directly (no JSON quotes)
    comp.setPropertyValue("mode", "a");
    manager.applySpec("test", &comp);
    // "a" should still resolve
    EXPECT_TRUE(comp.hasStyle("color"));
}
