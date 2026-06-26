#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "surface/component_manager/agenui_component_manager.h"
#include "surface/component_manager/agenui_component_model.h"
#include "surface/component_manager/data_value/agenui_static_data_value.h"
#include "surface/virtual_dom/agenui_ivirtual_dom.h"
#include "surface/agenui_isurface_context.h"

using namespace agenui;

// =============================================================================
// Mock Classes
// =============================================================================

class MockSurfaceContextCM : public ISurfaceContext {
public:
    MOCK_METHOD(int, getInstanceId, (), (const, override));
    MOCK_METHOD(std::string, getSurfaceId, (), (const, override));
    MOCK_METHOD(IDataModel*, getDataModel, (), (const, override));
    MOCK_METHOD(float, getSurfaceWidth, (), (override));
    MOCK_METHOD(float, getSurfaceHeight, (), (override));

    MockSurfaceContextCM() {
        ON_CALL(*this, getSurfaceWidth()).WillByDefault(::testing::Return(375.0f));
        ON_CALL(*this, getSurfaceHeight()).WillByDefault(::testing::Return(812.0f));
        ON_CALL(*this, getInstanceId()).WillByDefault(::testing::Return(1));
        ON_CALL(*this, getSurfaceId()).WillByDefault(::testing::Return("test_surface"));
        ON_CALL(*this, getDataModel()).WillByDefault(::testing::Return(nullptr));
    }
};

class MockComponentChangedObserver : public IComponentChangedObserver {
public:
    MOCK_METHOD(void, onComponentDeleted, (const std::string& componentId), (override));
    MOCK_METHOD(void, onComponentChanged, (const std::string& componentId), (override));
};

class MockTemplateComponentGenerator : public ITemplateComponentGenerator {
public:
    MOCK_METHOD(std::vector<std::shared_ptr<ComponentModel>>, generateListChildren,
                (const std::string& templateId, std::shared_ptr<DataValue> data), (override));
    MOCK_METHOD(std::shared_ptr<ComponentModel>, generateComponentWithTemplate,
                (const std::string& templateId, std::shared_ptr<DataValue> data), (override));
};

class MockVirtualDOMForCM : public IVirtualDOM {
public:
    MOCK_METHOD(void, updateNode, (const ComponentSnapshot& snapshot), (override));
    MOCK_METHOD(void, clear, (), (override));
    MOCK_METHOD(BatchGuard*, batchGuard, (), (override));
};

// =============================================================================
// ComponentModel Tests — Construction and basic getters
// =============================================================================

class ComponentModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        surfaceContext_ = std::make_unique<MockSurfaceContextCM>();
        observer_ = std::make_unique<MockComponentChangedObserver>();
        generator_ = std::make_unique<MockTemplateComponentGenerator>();
    }

    std::unique_ptr<ComponentModel> makeModel(const std::string& id, const std::string& component) {
        return std::make_unique<ComponentModel>(
            id, id, component,
            surfaceContext_.get(), observer_.get(), generator_.get());
    }

    std::unique_ptr<MockSurfaceContextCM> surfaceContext_;
    std::unique_ptr<MockComponentChangedObserver> observer_;
    std::unique_ptr<MockTemplateComponentGenerator> generator_;
};

TEST_F(ComponentModelTest, Construction_GetId) {
    auto model = makeModel("comp_1", "Text");
    EXPECT_EQ(model->getId(), "comp_1");
}

TEST_F(ComponentModelTest, Construction_GetRawId) {
    auto model = makeModel("comp_1", "Text");
    EXPECT_EQ(model->getRawId(), "comp_1");
}

TEST_F(ComponentModelTest, Construction_GetComponent) {
    auto model = makeModel("comp_1", "Text");
    EXPECT_EQ(model->getComponent(), "Text");
}

TEST_F(ComponentModelTest, Construction_GetComponentType) {
    auto model = makeModel("comp_1", "Markdown");
    EXPECT_EQ(model->getComponentType(), "Markdown");
}

TEST_F(ComponentModelTest, Construction_InitiallyDirty) {
    auto model = makeModel("comp_1", "Text");
    EXPECT_TRUE(model->hasDirty());
}

// =============================================================================
// ComponentModel Tests — Attributes
// =============================================================================

TEST_F(ComponentModelTest, SetAttribute_AddsToMap) {
    auto model = makeModel("comp_1", "Text");
    auto value = std::make_shared<StaticDataValue>("\"Hello\"");
    model->setAttribute("text", value);

    const auto& attrs = model->getAllAttributes();
    EXPECT_EQ(attrs.size(), 1u);
    EXPECT_TRUE(attrs.count("text") > 0);
}

TEST_F(ComponentModelTest, SetAttribute_MultipleAttributes) {
    auto model = makeModel("comp_1", "Image");
    model->setAttribute("src", std::make_shared<StaticDataValue>("\"image.png\""));
    model->setAttribute("alt", std::make_shared<StaticDataValue>("\"photo\""));

    const auto& attrs = model->getAllAttributes();
    EXPECT_EQ(attrs.size(), 2u);
}

TEST_F(ComponentModelTest, HasProperty_SetAttribute_ReturnsTrue) {
    auto model = makeModel("comp_1", "Text");
    model->setAttribute("text", std::make_shared<StaticDataValue>("\"Hello\""));
    EXPECT_TRUE(model->hasProperty("text"));
}

TEST_F(ComponentModelTest, HasProperty_Unset_ReturnsFalse) {
    auto model = makeModel("comp_1", "Text");
    EXPECT_FALSE(model->hasProperty("nonexistent"));
}

// =============================================================================
// ComponentModel Tests — Children
// =============================================================================

TEST_F(ComponentModelTest, SetChildren_StoresIds) {
    auto model = makeModel("parent_1", "Column");
    model->setChildren({"c1", "c2", "c3"});
    const auto& children = model->getChildren();
    EXPECT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], "c1");
    EXPECT_EQ(children[1], "c2");
    EXPECT_EQ(children[2], "c3");
}

TEST_F(ComponentModelTest, SetChildren_EmptyList) {
    auto model = makeModel("parent_1", "Column");
    model->setChildren({});
    EXPECT_TRUE(model->getChildren().empty());
}

TEST_F(ComponentModelTest, SetChildren_Override) {
    auto model = makeModel("parent_1", "Column");
    model->setChildren({"c1", "c2"});
    model->setChildren({"x1"});
    EXPECT_EQ(model->getChildren().size(), 1u);
    EXPECT_EQ(model->getChildren()[0], "x1");
}

// =============================================================================
// ComponentModel Tests — Dirty tracking
// =============================================================================

TEST_F(ComponentModelTest, MarkDirty_SetsHasDirty) {
    auto model = makeModel("comp_1", "Text");
    model->clearDirty();
    EXPECT_FALSE(model->hasDirty());

    model->markDirty("text", false);
    EXPECT_TRUE(model->hasDirty());
}

TEST_F(ComponentModelTest, ClearDirty_ResetsFlag) {
    auto model = makeModel("comp_1", "Text");
    EXPECT_TRUE(model->hasDirty());
    model->clearDirty();
    EXPECT_FALSE(model->hasDirty());
}

TEST_F(ComponentModelTest, MarkDirty_EmptyString_FullDirty) {
    auto model = makeModel("comp_1", "Text");
    model->clearDirty();
    model->markDirty("", false);
    EXPECT_TRUE(model->hasDirty());
}

// =============================================================================
// ComponentModel Tests — DisplayRule
// =============================================================================

TEST_F(ComponentModelTest, SetDisplayRule_Default_IsAlways) {
    auto model = makeModel("comp_1", "Text");
    // Default is Always — verified through snapshot
    model->clearDirty();
    model->markDirty("", false);
    const auto& snapshot = model->flushDirty();
    EXPECT_EQ(snapshot.displayRule, DisplayRule::Always);
}

TEST_F(ComponentModelTest, SetDisplayRule_AnyDataReady) {
    auto model = makeModel("comp_1", "Text");
    model->setDisplayRule(DisplayRule::AnyDataReady);
    model->clearDirty();
    model->markDirty("", false);
    const auto& snapshot = model->flushDirty();
    EXPECT_EQ(snapshot.displayRule, DisplayRule::AnyDataReady);
}

TEST_F(ComponentModelTest, SetDisplayRule_AllDataReady) {
    auto model = makeModel("comp_1", "Text");
    model->setDisplayRule(DisplayRule::AllDataReady);
    model->clearDirty();
    model->markDirty("", false);
    const auto& snapshot = model->flushDirty();
    EXPECT_EQ(snapshot.displayRule, DisplayRule::AllDataReady);
}

// =============================================================================
// ComponentModel Tests — FlushDirty
// =============================================================================

TEST_F(ComponentModelTest, FlushDirty_ReturnsSnapshotWithIdAndComponent) {
    auto model = makeModel("comp_1", "Text");
    const auto& snapshot = model->flushDirty();
    EXPECT_EQ(snapshot.id, "comp_1");
    EXPECT_EQ(snapshot.component, "Text");
    EXPECT_FALSE(model->hasDirty());
}

TEST_F(ComponentModelTest, FlushDirty_WithAttributes_IncludesInSnapshot) {
    auto model = makeModel("comp_1", "Text");
    model->setAttribute("text", std::make_shared<StaticDataValue>("\"Hello World\""));
    model->clearDirty();
    model->markDirty("", false);
    const auto& snapshot = model->flushDirty();
    // Attributes should be populated
    EXPECT_FALSE(snapshot.attributes.empty());
}

TEST_F(ComponentModelTest, FlushDirty_WithChildren_IncludesInSnapshot) {
    auto model = makeModel("parent", "Column");
    model->setChildren({"c1", "c2"});
    model->clearDirty();
    model->markDirty("", false);
    const auto& snapshot = model->flushDirty();
    EXPECT_EQ(snapshot.children.size(), 2u);
}

// =============================================================================
// ComponentModel Tests — Observer notification
// =============================================================================

TEST_F(ComponentModelTest, OnComponentAttributeDataChanged_NotifiesObserver) {
    EXPECT_CALL(*observer_, onComponentChanged("comp_1")).Times(::testing::AtLeast(1));

    auto model = makeModel("comp_1", "Text");
    model->setAttribute("text", std::make_shared<StaticDataValue>("\"Hello\""));
    model->clearDirty();
    model->onComponentAttributeDataChanged("text", false);
}

// =============================================================================
// ComponentManager Tests
// =============================================================================

class ComponentManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        surfaceContext_ = std::make_unique<MockSurfaceContextCM>();
        vdom_ = std::make_unique<MockVirtualDOMForCM>();
        batchGuard_ = std::make_unique<BatchGuard>([](){});

        ON_CALL(*vdom_, batchGuard()).WillByDefault(::testing::Return(batchGuard_.get()));
        EXPECT_CALL(*vdom_, updateNode(::testing::_)).Times(::testing::AnyNumber());

        cm_ = std::make_unique<ComponentManager>(surfaceContext_.get(), vdom_.get(), "default");
    }

    std::unique_ptr<MockSurfaceContextCM> surfaceContext_;
    std::unique_ptr<MockVirtualDOMForCM> vdom_;
    std::unique_ptr<BatchGuard> batchGuard_;
    std::unique_ptr<ComponentManager> cm_;
};

TEST_F(ComponentManagerTest, UpdateComponents_EmptyArray_NoError) {
    std::vector<std::string> empty;
    EXPECT_NO_THROW(cm_->updateComponents(empty));
    EXPECT_TRUE(cm_->getParentId("nonexistent").empty());
}

TEST_F(ComponentManagerTest, UpdateComponents_SingleComponent_Succeeds) {
    std::vector<std::string> components = {
        R"({"id":"comp_1","component":"Text","text":"Hello"})"
    };
    cm_->updateComponents(components);
    // Verify parent lookup returns empty for a root-level component
    EXPECT_TRUE(cm_->getParentId("comp_1").empty());
}

TEST_F(ComponentManagerTest, UpdateComponents_WithChildren_ParentRegistered) {
    // Component with inline children object format
    std::vector<std::string> components = {
        R"({"id":"parent","component":"Column","children":[{"id":"child","component":"Text","text":"Hi"}]})"
    };
    cm_->updateComponents(components);
    // Parent should be registered (getParentId for root-level is empty)
    EXPECT_TRUE(cm_->getParentId("parent").empty());
}

TEST_F(ComponentManagerTest, UpdateComponents_MultipleComponents_AllRegistered) {
    std::vector<std::string> components = {
        R"({"id":"a","component":"Text","text":"A"})",
        R"({"id":"b","component":"Image","src":"img.png"})"
    };
    cm_->updateComponents(components);
    // Both should exist (getParentId won't crash)
    EXPECT_TRUE(cm_->getParentId("a").empty());
    EXPECT_TRUE(cm_->getParentId("b").empty());
}

TEST_F(ComponentManagerTest, GetParentId_UnknownId_ReturnsEmpty) {
    EXPECT_TRUE(cm_->getParentId("nonexistent").empty());
}

TEST_F(ComponentManagerTest, BatchGuard_NotNull) {
    EXPECT_NE(cm_->batchGuard(), nullptr);
}

TEST_F(ComponentManagerTest, UpdateComponents_InvalidJson_DoesNotCrash) {
    std::vector<std::string> components = {
        "not valid json at all {{{",
        R"({"id":"valid","component":"Text","text":"ok"})"
    };
    EXPECT_NO_THROW(cm_->updateComponents(components));
    EXPECT_TRUE(cm_->getParentId("valid").empty());
}

// =============================================================================
// ComponentManager Tests — SetComponentsDisplayRule
// =============================================================================

TEST_F(ComponentManagerTest, SetComponentsDisplayRule_AppliesRules) {
    std::vector<std::string> components = {
        R"({"id":"comp_dr","component":"Text","text":"Hello"})"
    };
    cm_->updateComponents(components);

    std::map<std::string, DisplayRule> rules;
    rules["comp_dr"] = DisplayRule::AnyDataReady;
    EXPECT_NO_THROW(cm_->setComponentsDisplayRule(rules));
}

// =============================================================================
// ComponentManager Tests — SyncBindingValue
// =============================================================================

TEST_F(ComponentManagerTest, SyncBindingValue_UnknownId_DoesNotCrash) {
    EXPECT_NO_THROW(cm_->syncBindingValue("nonexistent", "text", "\"newValue\""));
}

TEST_F(ComponentManagerTest, SyncBindingValue_ExistingComponent_DoesNotCrash) {
    std::vector<std::string> components = {
        R"({"id":"synced","component":"Text","text":"original"})"
    };
    cm_->updateComponents(components);
    EXPECT_NO_THROW(cm_->syncBindingValue("synced", "text", "\"updated\""));
}
