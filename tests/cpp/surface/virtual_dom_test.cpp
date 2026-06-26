#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "surface/virtual_dom/agenui_virtual_dom.h"
#include "surface/virtual_dom/agenui_virtual_dom_node.h"
#include "surface/virtual_dom/agenui_component_snapshot.h"
#include "surface/virtual_dom/agenui_virtual_dom_observer.h"
#include "surface/agenui_isurface_context.h"

using namespace agenui;

// =============================================================================
// Mock Classes
// =============================================================================

class MockVirtualDOMObserver : public IVirtualDOMObserver {
public:
    MOCK_METHOD(void, onNodeUpdate, (const std::string& componentId, const std::string& nodeJson), (override));
    MOCK_METHOD(void, onNodeAdded, (const std::string& parentId, const std::string& nodeJson), (override));
    MOCK_METHOD(void, onNodeRemoved, (const std::string& parentId, const std::string& id), (override));
};

class MockSurfaceContext : public ISurfaceContext {
public:
    MOCK_METHOD(int, getInstanceId, (), (const, override));
    MOCK_METHOD(std::string, getSurfaceId, (), (const, override));
    MOCK_METHOD(IDataModel*, getDataModel, (), (const, override));
    MOCK_METHOD(float, getSurfaceWidth, (), (override));
    MOCK_METHOD(float, getSurfaceHeight, (), (override));

    MockSurfaceContext() {
        ON_CALL(*this, getSurfaceWidth()).WillByDefault(::testing::Return(375.0f));
        ON_CALL(*this, getSurfaceHeight()).WillByDefault(::testing::Return(812.0f));
        ON_CALL(*this, getInstanceId()).WillByDefault(::testing::Return(1));
        ON_CALL(*this, getSurfaceId()).WillByDefault(::testing::Return("test_surface"));
        ON_CALL(*this, getDataModel()).WillByDefault(::testing::Return(nullptr));
    }
};

// =============================================================================
// LayoutInfo Tests
// =============================================================================

TEST(LayoutInfo, DefaultConstructed_IsInvalid) {
    LayoutInfo info;
    EXPECT_FALSE(info.isValid());
    EXPECT_FLOAT_EQ(info.x, 0.0f);
    EXPECT_FLOAT_EQ(info.y, 0.0f);
    EXPECT_FLOAT_EQ(info.width, 0.0f);
    EXPECT_FLOAT_EQ(info.height, 0.0f);
}

TEST(LayoutInfo, WithPositiveWidth_IsValid) {
    LayoutInfo info;
    info.width = 100.0f;
    EXPECT_TRUE(info.isValid());
}

TEST(LayoutInfo, WithPositiveHeight_IsValid) {
    LayoutInfo info;
    info.height = 50.0f;
    EXPECT_TRUE(info.isValid());
}

TEST(LayoutInfo, Equality_SameValues_ReturnsTrue) {
    LayoutInfo a{10.0f, 20.0f, 100.0f, 50.0f};
    LayoutInfo b{10.0f, 20.0f, 100.0f, 50.0f};
    EXPECT_EQ(a, b);
}

TEST(LayoutInfo, Equality_DifferentValues_ReturnsFalse) {
    LayoutInfo a{10.0f, 20.0f, 100.0f, 50.0f};
    LayoutInfo b{10.0f, 20.0f, 100.0f, 60.0f};
    EXPECT_NE(a, b);
}

// =============================================================================
// ComponentSnapshot Tests
// =============================================================================

TEST(ComponentSnapshot, DefaultConstructed_EmptyFields) {
    ComponentSnapshot snapshot;
    EXPECT_TRUE(snapshot.id.empty());
    EXPECT_TRUE(snapshot.component.empty());
    EXPECT_TRUE(snapshot.children.empty());
    EXPECT_TRUE(snapshot.attributes.empty());
    EXPECT_TRUE(snapshot.styles.empty());
    EXPECT_EQ(snapshot.dataBindingStatus, DataBindingStatus::NotDependent);
    EXPECT_EQ(snapshot.displayRule, DisplayRule::Always);
    EXPECT_FALSE(snapshot.appendMode);
}

TEST(ComponentSnapshot, Stringify_BasicComponent_ContainsIdAndComponent) {
    ComponentSnapshot snapshot;
    snapshot.id = "comp_1";
    snapshot.component = "Text";

    std::string json = snapshot.stringify();
    EXPECT_NE(json.find("comp_1"), std::string::npos);
    EXPECT_NE(json.find("Text"), std::string::npos);
}

TEST(ComponentSnapshot, Stringify_WithChildren_ContainsChildrenArray) {
    ComponentSnapshot snapshot;
    snapshot.id = "parent_1";
    snapshot.component = "Column";
    snapshot.children.push_back("child_1");
    snapshot.children.push_back("child_2");

    std::string json = snapshot.stringify();
    EXPECT_NE(json.find("child_1"), std::string::npos);
    EXPECT_NE(json.find("child_2"), std::string::npos);
}

TEST(ComponentSnapshot, ResetMode_ClearsAppendMode) {
    ComponentSnapshot snapshot;
    snapshot.appendMode = true;
    snapshot.resetMode();
    EXPECT_FALSE(snapshot.appendMode);
}

TEST(ComponentSnapshot, Stringify_WithLayout_ContainsPositionAndSize) {
    ComponentSnapshot snapshot;
    snapshot.id = "comp_1";
    snapshot.component = "Box";
    snapshot.layout.x = 10.0f;
    snapshot.layout.y = 20.0f;
    snapshot.layout.width = 100.0f;
    snapshot.layout.height = 50.0f;

    std::string json = snapshot.stringify();
    // Layout values should appear in the styles section
    EXPECT_NE(json.find("styles"), std::string::npos);
}

// =============================================================================
// VirtualDOM Tests — Construction
// =============================================================================

class VirtualDOMTest : public ::testing::Test {
protected:
    void SetUp() override {
        observer_ = std::make_unique<MockVirtualDOMObserver>();
        surfaceContext_ = std::make_unique<MockSurfaceContext>();
        vdom_ = std::make_unique<VirtualDOM>(observer_.get(), surfaceContext_.get(), nullptr);
    }

    std::unique_ptr<MockVirtualDOMObserver> observer_;
    std::unique_ptr<MockSurfaceContext> surfaceContext_;
    std::unique_ptr<VirtualDOM> vdom_;

    ComponentSnapshot makeSnapshot(const std::string& id, const std::string& component,
                                   std::vector<std::string> children = {}) {
        ComponentSnapshot s;
        s.id = id;
        s.component = component;
        s.children = std::move(children);
        return s;
    }
};

TEST_F(VirtualDOMTest, Construction_HasRoot) {
    auto root = vdom_->getRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getId(), "root");
}

TEST_F(VirtualDOMTest, Construction_RootHasNoSnapshot) {
    auto root = vdom_->getRoot();
    EXPECT_FALSE(root->hasSnapshot());
}

TEST_F(VirtualDOMTest, BatchGuard_NotNull) {
    EXPECT_NE(vdom_->batchGuard(), nullptr);
}

// =============================================================================
// VirtualDOM Tests — updateNode
// =============================================================================

TEST_F(VirtualDOMTest, UpdateNode_Root_SetsRootSnapshot) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto snapshot = makeSnapshot("root", "Column", {"child_1"});
    vdom_->updateNode(snapshot);

    auto root = vdom_->getRoot();
    ASSERT_TRUE(root->hasSnapshot());
    EXPECT_EQ(root->getSnapshot()->component, "Column");
}

TEST_F(VirtualDOMTest, UpdateNode_Root_WithChildren_CreatesChildNodes) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto snapshot = makeSnapshot("root", "Column", {"child_1", "child_2"});
    vdom_->updateNode(snapshot);

    auto root = vdom_->getRoot();
    EXPECT_EQ(root->getChildren().size(), 2u);
}

TEST_F(VirtualDOMTest, UpdateNode_UnknownId_StoredAsOrphan) {
    // Update a node that doesn't exist in the tree — should be stored as orphan
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto snapshot = makeSnapshot("non_existent", "Text");
    vdom_->updateNode(snapshot);

    // Verify it was stored as orphan by trying to take it
    ComponentSnapshot retrieved;
    bool found = vdom_->takeOrphanSnapshot("non_existent", retrieved);
    EXPECT_TRUE(found);
    EXPECT_EQ(retrieved.component, "Text");
}

TEST_F(VirtualDOMTest, UpdateNode_ExistingChild_UpdatesSnapshot) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    // Build root with a child
    auto rootSnap = makeSnapshot("root", "Column", {"child_1"});
    vdom_->updateNode(rootSnap);

    // Provide snapshot for child
    auto childSnap = makeSnapshot("child_1", "Text");
    vdom_->updateNode(childSnap);

    // Update child with different component type
    auto childUpdate = makeSnapshot("child_1", "Markdown");
    vdom_->updateNode(childUpdate);

    // Verify the child was updated
    auto root = vdom_->getRoot();
    ASSERT_EQ(root->getChildren().size(), 1u);
    auto child = root->getChildren()[0];
    ASSERT_TRUE(child->hasSnapshot());
    EXPECT_EQ(child->getSnapshot()->component, "Markdown");
}

// =============================================================================
// VirtualDOM Tests — clear
// =============================================================================

TEST_F(VirtualDOMTest, Clear_ResetsTree) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeRemoved(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto snapshot = makeSnapshot("root", "Column", {"c1", "c2"});
    vdom_->updateNode(snapshot);

    vdom_->clear();

    auto root = vdom_->getRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getId(), "root");
    EXPECT_FALSE(root->hasSnapshot());
    EXPECT_TRUE(root->getChildren().empty());
}

TEST_F(VirtualDOMTest, Clear_OrphansAlsoCleared) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto orphanSnap = makeSnapshot("orphan_1", "Button");
    vdom_->updateNode(orphanSnap);

    vdom_->clear();

    ComponentSnapshot retrieved;
    EXPECT_FALSE(vdom_->takeOrphanSnapshot("orphan_1", retrieved));
}

// =============================================================================
// VirtualDOM Tests — takeOrphanSnapshot
// =============================================================================

TEST_F(VirtualDOMTest, TakeOrphanSnapshot_NotExist_ReturnsFalse) {
    ComponentSnapshot retrieved;
    EXPECT_FALSE(vdom_->takeOrphanSnapshot("nothing", retrieved));
}

TEST_F(VirtualDOMTest, TakeOrphanSnapshot_Exists_ReturnsTrueAndRemoves) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto snap = makeSnapshot("orphan_x", "Image");
    vdom_->updateNode(snap);

    ComponentSnapshot retrieved;
    EXPECT_TRUE(vdom_->takeOrphanSnapshot("orphan_x", retrieved));
    EXPECT_EQ(retrieved.component, "Image");

    // Second attempt should fail (already removed)
    ComponentSnapshot second;
    EXPECT_FALSE(vdom_->takeOrphanSnapshot("orphan_x", second));
}

// =============================================================================
// VirtualDOM Tests — DisplayRule / DataBindingStatus
// =============================================================================

TEST_F(VirtualDOMTest, Orphan_DisplayRuleAlways_CanBeTaken) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    ComponentSnapshot snap;
    snap.id = "always_show";
    snap.component = "Text";
    snap.displayRule = DisplayRule::Always;
    snap.dataBindingStatus = DataBindingStatus::NotDependent;
    vdom_->updateNode(snap);

    ComponentSnapshot retrieved;
    EXPECT_TRUE(vdom_->takeOrphanSnapshot("always_show", retrieved));
}

TEST_F(VirtualDOMTest, Orphan_AllDataReady_NotReadyStatus_CannotBeTaken) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    ComponentSnapshot snap;
    snap.id = "needs_data";
    snap.component = "Text";
    snap.displayRule = DisplayRule::AllDataReady;
    snap.dataBindingStatus = DataBindingStatus::NotReady;
    vdom_->updateNode(snap);

    ComponentSnapshot retrieved;
    EXPECT_FALSE(vdom_->takeOrphanSnapshot("needs_data", retrieved));
}

TEST_F(VirtualDOMTest, Orphan_AnyDataReady_FullyReadyStatus_CanBeTaken) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    ComponentSnapshot snap;
    snap.id = "partial_ok";
    snap.component = "Markdown";
    snap.displayRule = DisplayRule::AnyDataReady;
    snap.dataBindingStatus = DataBindingStatus::FullyReady;
    vdom_->updateNode(snap);

    ComponentSnapshot retrieved;
    EXPECT_TRUE(vdom_->takeOrphanSnapshot("partial_ok", retrieved));
}

// =============================================================================
// VirtualDOM Tests — Batch operations
// =============================================================================

TEST_F(VirtualDOMTest, BatchScope_CoalescesMultipleUpdates) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    // Within a batch, multiple updateNode calls should coalesce layout
    {
        BatchScope scope(vdom_->batchGuard());
        auto rootSnap = makeSnapshot("root", "Column", {"a", "b", "c"});
        vdom_->updateNode(rootSnap);
        vdom_->updateNode(makeSnapshot("a", "Text"));
        vdom_->updateNode(makeSnapshot("b", "Image"));
        vdom_->updateNode(makeSnapshot("c", "Button"));
    }
    // After scope closes, all children should exist
    auto root = vdom_->getRoot();
    EXPECT_EQ(root->getChildren().size(), 3u);
}

// =============================================================================
// VirtualDOMNode Tests
// =============================================================================

TEST_F(VirtualDOMTest, Node_FindChild_ExistingId_ReturnsNode) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto rootSnap = makeSnapshot("root", "Column", {"findme"});
    vdom_->updateNode(rootSnap);

    auto root = vdom_->getRoot();
    auto found = root->findChild("findme");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getId(), "findme");
}

TEST_F(VirtualDOMTest, Node_FindChild_NonExistingId_ReturnsNull) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto rootSnap = makeSnapshot("root", "Column", {"child_1"});
    vdom_->updateNode(rootSnap);

    auto root = vdom_->getRoot();
    auto found = root->findChild("nonexistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(VirtualDOMTest, Node_GetId_ReturnsCorrectId) {
    auto root = vdom_->getRoot();
    EXPECT_EQ(root->getId(), "root");
}

TEST_F(VirtualDOMTest, Node_HasSnapshot_InitiallyFalse) {
    auto root = vdom_->getRoot();
    EXPECT_FALSE(root->hasSnapshot());
}

TEST_F(VirtualDOMTest, Node_SetSnapshot_MakesHasSnapshotTrue) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    auto rootSnap = makeSnapshot("root", "Column");
    vdom_->updateNode(rootSnap);

    auto root = vdom_->getRoot();
    EXPECT_TRUE(root->hasSnapshot());
}

// =============================================================================
// VirtualDOM Tests — Deep tree operations
// =============================================================================

TEST_F(VirtualDOMTest, DeepTree_NestedChildren_FoundCorrectly) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    // root -> level1 -> level2
    vdom_->updateNode(makeSnapshot("root", "Column", {"level1"}));
    vdom_->updateNode(makeSnapshot("level1", "Row", {"level2"}));
    vdom_->updateNode(makeSnapshot("level2", "Text"));

    auto root = vdom_->getRoot();
    ASSERT_EQ(root->getChildren().size(), 1u);
    auto level1 = root->getChildren()[0];
    ASSERT_TRUE(level1->hasSnapshot());
    ASSERT_EQ(level1->getChildren().size(), 1u);
    auto level2 = level1->getChildren()[0];
    ASSERT_TRUE(level2->hasSnapshot());
    EXPECT_EQ(level2->getSnapshot()->component, "Text");
}

TEST_F(VirtualDOMTest, UpdateNode_ChildrenReordering_UpdatesCorrectly) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeRemoved(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    // Initial: root with children [a, b, c]
    vdom_->updateNode(makeSnapshot("root", "Column", {"a", "b", "c"}));
    vdom_->updateNode(makeSnapshot("a", "Text"));
    vdom_->updateNode(makeSnapshot("b", "Image"));
    vdom_->updateNode(makeSnapshot("c", "Button"));

    // Update root with reordered children [c, a]
    vdom_->updateNode(makeSnapshot("root", "Column", {"c", "a"}));

    auto root = vdom_->getRoot();
    EXPECT_EQ(root->getChildren().size(), 2u);
}

// =============================================================================
// ComponentSnapshot — DataBindingStatus and DisplayRule enum coverage
// =============================================================================

TEST(DataBindingStatus, AllEnumValues_Compile) {
    EXPECT_NE(DataBindingStatus::NotDependent, DataBindingStatus::NotReady);
    EXPECT_NE(DataBindingStatus::NotReady, DataBindingStatus::PartiallyReady);
    EXPECT_NE(DataBindingStatus::PartiallyReady, DataBindingStatus::FullyReady);
}

TEST(DisplayRule, AllEnumValues_Compile) {
    EXPECT_NE(DisplayRule::Always, DisplayRule::AnyDataReady);
    EXPECT_NE(DisplayRule::AnyDataReady, DisplayRule::AllDataReady);
}

// =============================================================================
// VirtualDOM Tests — Observer notification verification (AP-5 coverage)
// =============================================================================

class VirtualDOMObserverTest : public ::testing::Test {
protected:
    void SetUp() override {
        observer_ = std::make_unique<MockVirtualDOMObserver>();
        surfaceContext_ = std::make_unique<MockSurfaceContext>();
        vdom_ = std::make_unique<VirtualDOM>(observer_.get(), surfaceContext_.get(), nullptr);
    }

    std::unique_ptr<MockVirtualDOMObserver> observer_;
    std::unique_ptr<MockSurfaceContext> surfaceContext_;
    std::unique_ptr<VirtualDOM> vdom_;

    ComponentSnapshot makeSnapshot(const std::string& id, const std::string& component,
                                   std::vector<std::string> children = {}) {
        ComponentSnapshot s;
        s.id = id;
        s.component = component;
        s.children = std::move(children);
        return s;
    }
};

TEST_F(VirtualDOMObserverTest, UpdateNode_Root_NotifiesOnNodeAdded) {
    EXPECT_CALL(*observer_, onNodeAdded("", ::testing::_)).Times(1);

    vdom_->updateNode(makeSnapshot("root", "Column"));
}

TEST_F(VirtualDOMObserverTest, UpdateNode_ChildSnapshot_NotifiesOnNodeAdded) {
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    vdom_->updateNode(makeSnapshot("root", "Column", {"child_a", "child_b"}));
    ::testing::Mock::VerifyAndClearExpectations(observer_.get());

    EXPECT_CALL(*observer_, onNodeAdded("root", ::testing::_)).Times(2);
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    vdom_->updateNode(makeSnapshot("child_a", "Text"));
    vdom_->updateNode(makeSnapshot("child_b", "Image"));
}

TEST_F(VirtualDOMObserverTest, UpdateNode_ExistingChild_NotifiesUpdateNotAdd) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    vdom_->updateNode(makeSnapshot("root", "Column", {"child_1"}));
    vdom_->updateNode(makeSnapshot("child_1", "Text"));

    ::testing::Mock::VerifyAndClearExpectations(observer_.get());

    EXPECT_CALL(*observer_, onNodeUpdate("child_1", ::testing::_)).Times(1);
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(0);

    vdom_->updateNode(makeSnapshot("child_1", "Markdown"));
}

TEST_F(VirtualDOMObserverTest, Clear_NotifiesOnNodeRemovedForRoot) {
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    vdom_->updateNode(makeSnapshot("root", "Column"));

    ::testing::Mock::VerifyAndClearExpectations(observer_.get());

    EXPECT_CALL(*observer_, onNodeRemoved("", "root")).Times(1);

    vdom_->clear();
}

TEST_F(VirtualDOMObserverTest, ChildrenReorder_NotifiesRemovedForDroppedChild) {
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    vdom_->updateNode(makeSnapshot("root", "Column", {"a", "b", "c"}));
    vdom_->updateNode(makeSnapshot("a", "Text"));
    vdom_->updateNode(makeSnapshot("b", "Image"));
    vdom_->updateNode(makeSnapshot("c", "Button"));

    ::testing::Mock::VerifyAndClearExpectations(observer_.get());

    EXPECT_CALL(*observer_, onNodeRemoved(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeRemoved("root", "b")).Times(1);
    EXPECT_CALL(*observer_, onNodeUpdate(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    EXPECT_CALL(*observer_, onNodeAdded(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

    vdom_->updateNode(makeSnapshot("root", "Column", {"a", "c"}));
}
