#include <gtest/gtest.h>
#include "surface/agenui_template_registry.h"

using namespace agenui;

// ═══════════════════════════════════════════════════════════════════════════════════
// TemplateRegistry Unit Tests
// ═══════════════════════════════════════════════════════════════════════════════════

class TemplateRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<TemplateRegistry>();
    }

    std::unique_ptr<TemplateRegistry> registry;
};

// --- Lifecycle ---

TEST_F(TemplateRegistryTest, Construction_NoTemplatesLoaded) {
    // Before initialize, isTemplate should return false for any name
    // (loadTemplateRegistry will attempt to load but path is empty → no templates)
    EXPECT_FALSE(registry->isTemplate("Text"));
    EXPECT_FALSE(registry->isTemplate("Card"));
}

TEST_F(TemplateRegistryTest, Initialize_ReturnsTrue) {
    EXPECT_TRUE(registry->initialize());
}

TEST_F(TemplateRegistryTest, Start_ReturnsTrue) {
    registry->initialize();
    EXPECT_TRUE(registry->start());
}

TEST_F(TemplateRegistryTest, StopAndShutdown_NoCrash) {
    registry->initialize();
    registry->start();
    registry->stop();
    registry->shutdown();
    // After shutdown, isTemplate should return false
    EXPECT_FALSE(registry->isTemplate("AnyTemplate"));
}

TEST_F(TemplateRegistryTest, DoubleInitialize_NoCrash) {
    EXPECT_TRUE(registry->initialize());
    EXPECT_TRUE(registry->initialize());
}

TEST_F(TemplateRegistryTest, ShutdownWithoutStart_NoCrash) {
    registry->shutdown();
}

// --- isTemplate without templates loaded ---

TEST_F(TemplateRegistryTest, IsTemplate_EmptyString_ReturnsFalse) {
    registry->initialize();
    registry->start();
    EXPECT_FALSE(registry->isTemplate(""));
}

TEST_F(TemplateRegistryTest, IsTemplate_RandomName_ReturnsFalse) {
    registry->initialize();
    registry->start();
    EXPECT_FALSE(registry->isTemplate("NonExistentTemplate"));
}

TEST_F(TemplateRegistryTest, IsTemplate_BuiltinComponent_ReturnsFalse) {
    registry->initialize();
    registry->start();
    // Built-in components should not be templates
    EXPECT_FALSE(registry->isTemplate("Text"));
    EXPECT_FALSE(registry->isTemplate("Button"));
    EXPECT_FALSE(registry->isTemplate("Card"));
    EXPECT_FALSE(registry->isTemplate("Image"));
}

// --- expandTemplate with no templates ---

TEST_F(TemplateRegistryTest, ExpandTemplate_EmptyComponent_ReturnsEmpty) {
    registry->initialize();
    registry->start();
    nlohmann::json emptyComponent = nlohmann::json::object();
    auto result = registry->expandTemplate(emptyComponent);
    EXPECT_TRUE(result.components.empty());
}

TEST_F(TemplateRegistryTest, ExpandTemplate_NonTemplateComponent_ReturnsEmpty) {
    registry->initialize();
    registry->start();
    nlohmann::json component = {
        {"component", "Text"},
        {"id", "text1"},
        {"properties", {{"text", "hello"}}}
    };
    auto result = registry->expandTemplate(component);
    // Non-template components are not expanded — returns empty
    EXPECT_TRUE(result.components.empty());
}

// --- ExpandedTemplate struct ---

TEST_F(TemplateRegistryTest, ExpandedTemplate_DefaultConstruction) {
    ExpandedTemplate expanded;
    EXPECT_TRUE(expanded.components.empty());
    EXPECT_TRUE(expanded.displayRules.empty());
}
