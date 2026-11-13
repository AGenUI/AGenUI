#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "surface/component_manager/agenui_component_manager.h"
#include "surface/component_manager/agenui_component_model.h"
#include "surface/component_manager/data_value/agenui_static_data_value.h"
#include "surface/component_manager/data_value/agenui_interpolation_expression_data_value.h"
#include "surface/component_manager/data_value/agenui_data_value_parser.h"
#include "surface/datamodel/agenui_data_model.h"
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

// =============================================================================
// ComponentModel Tests — syncValue static (literal) attribute write-back
//
// Host custom components (AmapCheckBox / Switch) call syncState({"value": ...})
// after the user toggles the control. The core must update the static
// attribute baseline so a later business push of the original value produces
// a real downstream diff. See plan 三端静态状态回写修复.
// =============================================================================

// False -> true write-back must update the attribute, notify the observer and
// reflect in the flushed snapshot.
TEST_F(ComponentModelTest, SyncValue_StaticData_Bool_UpdatesAndNotifies) {
    auto model = makeModel("cb_1", "AmapCheckBox");
    model->setAttribute("value", std::make_shared<StaticDataValue>("false"));
    model->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("cb_1")).Times(1);

    model->syncValue("value", "true");

    const auto& attrs = model->getAllAttributes();
    ASSERT_EQ(attrs.count("value"), 1u);
    EXPECT_TRUE(attrs.at("value")->getValueData().asBool());

    model->markDirty("value", false);
    const auto& snapshot = model->flushDirty();
    ASSERT_EQ(snapshot.attributes.count("value"), 1u);
    EXPECT_TRUE(snapshot.attributes.at("value").asBool());
}

// Same-value write-back must be a no-op: no notification, no snapshot churn.
TEST_F(ComponentModelTest, SyncValue_StaticData_SameValue_NoNotify) {
    auto model = makeModel("cb_2", "AmapCheckBox");
    model->setAttribute("value", std::make_shared<StaticDataValue>("false"));
    model->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("cb_2")).Times(0);

    model->syncValue("value", "false");
    EXPECT_FALSE(model->hasDirty());
}

// Structural comparison: whitespace-only differences and object key order
// must not be treated as a change.
TEST_F(ComponentModelTest, SyncValue_StaticData_EquivalentJson_NoNotify) {
    auto model = makeModel("sw_1", "Switch");
    model->setAttribute("value", std::make_shared<StaticDataValue>("false"));
    model->setAttribute("payload", std::make_shared<StaticDataValue>(R"({"a":1,"b":2})"));
    model->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("sw_1")).Times(0);

    model->syncValue("value", "  false  ");
    model->syncValue("payload", R"({"b":2,"a":1})");
    EXPECT_FALSE(model->hasDirty());
}

// All JSON value kinds are accepted verbatim; null keeps the attribute
// present with a null value (no type coercion, no attribute deletion).
TEST_F(ComponentModelTest, SyncValue_StaticData_ValueKinds_IncludingNull) {
    auto model = makeModel("sw_2", "Switch");
    model->setAttribute("value", std::make_shared<StaticDataValue>("false"));
    model->clearDirty();

    // number
    model->syncValue("value", "3");
    EXPECT_EQ(model->getAllAttributes().at("value")->getValueData().asInt(), 3);

    // string (syncUIToData dumps strings with quotes)
    model->syncValue("value", "\"medium\"");
    EXPECT_EQ(model->getAllAttributes().at("value")->getValueData().asString(), "medium");

    // array
    model->syncValue("value", "[1,2]");
    EXPECT_EQ(model->getAllAttributes().at("value")->getValueData().size(), 2u);

    // object
    model->syncValue("value", R"({"k":"v"})");
    EXPECT_TRUE(model->getAllAttributes().at("value")->getValueData().isObject());

    // null: value kept, attribute declaration kept
    model->syncValue("value", "null");
    EXPECT_TRUE(model->hasProperty("value"));
    EXPECT_TRUE(model->getAllAttributes().at("value")->getValueData().isNull());
}

// Invalid JSON keeps the current state and must not notify.
TEST_F(ComponentModelTest, SyncValue_StaticData_InvalidJson_Ignored) {
    auto model = makeModel("cb_3", "AmapCheckBox");
    model->setAttribute("value", std::make_shared<StaticDataValue>("false"));
    model->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("cb_3")).Times(0);

    model->syncValue("value", "not json at all");
    EXPECT_FALSE(model->getAllAttributes().at("value")->getValueData().asBool());
    EXPECT_FALSE(model->hasDirty());
}

// A write-back for an attribute the component does not declare is ignored:
// syncValue must never create undeclared attributes.
TEST_F(ComponentModelTest, SyncValue_MissingAttribute_Ignored) {
    auto model = makeModel("cb_4", "AmapCheckBox");
    model->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("cb_4")).Times(0);

    model->syncValue("value", "true");
    EXPECT_FALSE(model->hasProperty("value"));
    EXPECT_FALSE(model->hasDirty());
}

// The replacement allocates a fresh StaticDataValue; other holders of the old
// one (e.g. sibling components sharing the literal) must keep their value.
TEST_F(ComponentModelTest, SyncValue_StaticData_DoesNotAffectOtherHolders) {
    auto shared = std::make_shared<StaticDataValue>("false");
    auto modelA = makeModel("cb_a", "AmapCheckBox");
    auto modelB = makeModel("cb_b", "AmapCheckBox");
    modelA->setAttribute("value", shared);
    modelB->setAttribute("value", shared);
    modelA->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("cb_a")).Times(1);
    EXPECT_CALL(*observer_, onComponentChanged("cb_b")).Times(0);

    modelA->syncValue("value", "true");

    EXPECT_TRUE(modelA->getAllAttributes().at("value")->getValueData().asBool());
    EXPECT_FALSE(modelB->getAllAttributes().at("value")->getValueData().asBool());
    // modelB still points at the original (shared) instance.
    EXPECT_EQ(modelB->getAllAttributes().at("value").get(), shared.get());
}

// Bound attributes keep their existing channel: the write goes to the
// DataModel at the binding path (never replacing the binding itself).
TEST_F(ComponentModelTest, SyncValue_BindingAttribute_UpdatesDataModel) {
    DataModel dataModel;
    ON_CALL(*surfaceContext_, getDataModel()).WillByDefault(::testing::Return(&dataModel));

    auto model = makeModel("cb_bind", "AmapCheckBox");
    model->setAttribute("value", DataValueParser::parseDataValue(model.get(), R"({"path":"/state/cb"})"));
    dataModel.updateData("/state/cb", "false");
    model->clearDirty();

    // The DataModel update flows back through AttributeDataBinder and
    // notifies the component-change observer exactly once.
    EXPECT_CALL(*observer_, onComponentChanged("cb_bind")).Times(1);

    model->syncValue("value", "true");

    EXPECT_TRUE(dataModel.getValue("/state/cb").asBool());
    // The attribute is still a binding, not a frozen literal.
    EXPECT_EQ(model->getAllAttributes().at("value")->getDataType(), DataType::DataBindingData);
}

// Dedicated DataValue types (function expressions, interpolation templates,
// structured composites) must never be replaced by a frozen constant.
TEST_F(ComponentModelTest, SyncValue_InterpolationAttribute_NotOverwritten) {
    auto model = makeModel("sw_3", "Switch");
    std::vector<std::shared_ptr<DataValue>> segments;
    segments.push_back(std::make_shared<StaticDataValue>("\"a\""));
    auto expression = std::make_shared<InterpolationExpressionDataValue>(model.get(), segments);
    model->setAttribute("value", expression);
    model->clearDirty();

    EXPECT_CALL(*observer_, onComponentChanged("sw_3")).Times(0);

    model->syncValue("value", "true");
    EXPECT_EQ(model->getAllAttributes().at("value").get(), expression.get());
}

// =============================================================================
// ComponentManager Tests — syncBindingValue pushes static write-back to VDOM
// =============================================================================

TEST_F(ComponentManagerTest, SyncBindingValue_StaticAttribute_PushesUpdateToVirtualDom) {
    std::vector<std::string> components = {
        R"({"id":"cb1","component":"AmapCheckBox","value":false})"
    };
    cm_->updateComponents(components);

    int updateCount = 0;
    SerializableData capturedValue;
    EXPECT_CALL(*vdom_, updateNode(::testing::_)).Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const ComponentSnapshot& snapshot) {
            updateCount++;
            auto it = snapshot.attributes.find("value");
            if (it != snapshot.attributes.end()) {
                capturedValue = it->second;
            }
        }));

    // User toggled the control on the platform side; the write-back must
    // reach the virtual DOM as a real snapshot update.
    cm_->syncBindingValue("cb1", "value", "true");

    EXPECT_EQ(updateCount, 1);
    ASSERT_EQ(capturedValue.isValid(), true);
    EXPECT_TRUE(capturedValue.asBool());
}

TEST_F(ComponentManagerTest, SyncBindingValue_StaticAttribute_SameValue_NoVirtualDomUpdate) {
    std::vector<std::string> components = {
        R"({"id":"cb2","component":"AmapCheckBox","value":false})"
    };
    cm_->updateComponents(components);

    int updateCount = 0;
    EXPECT_CALL(*vdom_, updateNode(::testing::_)).Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const ComponentSnapshot&) {
            updateCount++;
        }));

    // Re-pushing the value the user already has must deduplicate: no VDOM
    // update, hence no downstream diff.
    cm_->syncBindingValue("cb2", "value", "false");

    EXPECT_EQ(updateCount, 0);
}

TEST_F(ComponentManagerTest, SyncBindingValue_MissingAttribute_NoVirtualDomUpdate) {
    std::vector<std::string> components = {
        R"({"id":"cb3","component":"AmapCheckBox","value":false})"
    };
    cm_->updateComponents(components);

    int updateCount = 0;
    EXPECT_CALL(*vdom_, updateNode(::testing::_)).Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const ComponentSnapshot&) {
            updateCount++;
        }));

    // 'checked' is not a declared attribute on cb3: silently ignored.
    cm_->syncBindingValue("cb3", "checked", "true");

    EXPECT_EQ(updateCount, 0);
}
