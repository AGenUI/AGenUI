#include <gtest/gtest.h>
#include "surface/datamodel/agenui_data_model.h"
#include "surface/datamodel/agenui_data_observer.h"

using agenui::DataModel;
using agenui::IDataChangedObserver;
using agenui::SerializableData;

// ============================================================================
// Mock observer
// ============================================================================

class MockDataObserver : public IDataChangedObserver {
public:
    int callCount = 0;
    std::string lastPath;
    std::string lastValue;
    bool lastAppendMode = false;

    void onDataChanged(const std::string& path, const std::string& newValue, bool appendMode) override {
        callCount++;
        lastPath = path;
        lastValue = newValue;
        lastAppendMode = appendMode;
    }

    void reset() {
        callCount = 0;
        lastPath.clear();
        lastValue.clear();
        lastAppendMode = false;
    }
};

// ============================================================================
// DataModel basic operations
// ============================================================================

class DataModelTest : public ::testing::Test {
protected:
    DataModel model;
};

// ============================================================================
// updateData + getValue
// ============================================================================

TEST_F(DataModelTest, UpdateData_SetValueAtPath) {
    model.updateData("/name", "\"Alice\"");
    auto v = model.getValue("/name");
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.dump(), "\"Alice\"");
}

TEST_F(DataModelTest, UpdateData_SetNumberAtPath) {
    model.updateData("/count", "42");
    auto v = model.getValue("/count");
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.dump(), "42");
}

TEST_F(DataModelTest, UpdateData_SetObjectAtPath) {
    model.updateData("/user", R"({"name":"Bob","age":30})");
    auto v = model.getValue("/user");
    EXPECT_TRUE(v.isValid());
}

TEST_F(DataModelTest, UpdateData_SetArrayAtPath) {
    model.updateData("/items", R"([1,2,3])");
    auto v = model.getValue("/items");
    EXPECT_TRUE(v.isValid());
}

TEST_F(DataModelTest, UpdateData_SetBoolAtPath) {
    model.updateData("/flag", "true");
    auto v = model.getValue("/flag");
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.dump(), "true");
}

TEST_F(DataModelTest, UpdateData_SetNullAtPath) {
    model.updateData("/empty", "null");
    auto v = model.getValue("/empty");
    EXPECT_EQ(v.dump(), "null");
}

TEST_F(DataModelTest, UpdateData_OverwriteExisting) {
    model.updateData("/val", "1");
    model.updateData("/val", "2");
    auto v = model.getValue("/val");
    EXPECT_EQ(v.dump(), "2");
}

TEST_F(DataModelTest, UpdateData_RootMerge) {
    model.updateData("/", R"({"a":1,"b":2})");
    model.updateData("/", R"({"c":3})");
    auto va = model.getValue("/a");
    auto vc = model.getValue("/c");
    EXPECT_TRUE(va.isValid());
    EXPECT_TRUE(vc.isValid());
}

TEST_F(DataModelTest, UpdateData_NestedPath) {
    model.updateData("/user/name", "\"Alice\"");
    auto v = model.getValue("/user/name");
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.dump(), "\"Alice\"");
}

TEST_F(DataModelTest, UpdateData_EmptyPath_Ignored) {
    EXPECT_NO_THROW(model.updateData("", "\"value\""));
}

TEST_F(DataModelTest, UpdateData_NonJsonValue_StoredAsString) {
    model.updateData("/raw", "not json at all");
    auto v = model.getValue("/raw");
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.dump(), "\"not json at all\"");
}

// ============================================================================
// getValue edge cases
// ============================================================================

TEST_F(DataModelTest, GetValue_EmptyPath_ReturnsInvalid) {
    auto v = model.getValue("");
    EXPECT_FALSE(v.isValid());
}

TEST_F(DataModelTest, GetValue_PathWithoutSlash_ReturnsInvalid) {
    auto v = model.getValue("noSlash");
    EXPECT_FALSE(v.isValid());
}

TEST_F(DataModelTest, GetValue_NonExistentPath_ReturnsInvalid) {
    auto v = model.getValue("/does/not/exist");
    EXPECT_FALSE(v.isValid());
}

TEST_F(DataModelTest, GetValue_RootPath_ReturnsEntireRoot) {
    model.updateData("/a", "1");
    model.updateData("/b", "2");
    auto v = model.getValue("/");
    EXPECT_TRUE(v.isValid());
}

// ============================================================================
// appendData
// ============================================================================

TEST_F(DataModelTest, AppendData_MergesIntoExistingObject) {
    model.updateData("/obj", R"({"a":1})");
    model.appendData("/obj", R"({"b":2})");
    auto va = model.getValue("/obj/a");
    auto vb = model.getValue("/obj/b");
    EXPECT_TRUE(va.isValid());
    EXPECT_TRUE(vb.isValid());
}

TEST_F(DataModelTest, AppendData_NewPath_CreatesEntry) {
    model.appendData("/newkey", "\"hello\"");
    auto v = model.getValue("/newkey");
    EXPECT_TRUE(v.isValid());
}

TEST_F(DataModelTest, AppendData_RootMerge) {
    model.updateData("/", R"({"x":1})");
    model.appendData("/", R"({"y":2})");
    auto vx = model.getValue("/x");
    auto vy = model.getValue("/y");
    EXPECT_TRUE(vx.isValid());
    EXPECT_TRUE(vy.isValid());
}

TEST_F(DataModelTest, AppendData_EmptyPath_Ignored) {
    EXPECT_NO_THROW(model.appendData("", "\"value\""));
}

// ============================================================================
// bind / unbind / observer notification
// ============================================================================

TEST_F(DataModelTest, Bind_ObserverNotifiedOnUpdate) {
    MockDataObserver observer;
    model.bind("/name", &observer);

    model.updateData("/name", "\"Alice\"");
    EXPECT_EQ(observer.callCount, 1);
    EXPECT_EQ(observer.lastPath, "/name");

    model.unbind("/name", &observer);
}

TEST_F(DataModelTest, Unbind_ObserverNotNotifiedAfterUnbind) {
    MockDataObserver observer;
    model.bind("/name", &observer);
    model.unbind("/name", &observer);

    model.updateData("/name", "\"Bob\"");
    EXPECT_EQ(observer.callCount, 0);
}

TEST_F(DataModelTest, Bind_MultipleObserversOnSamePath) {
    MockDataObserver a, b;
    model.bind("/val", &a);
    model.bind("/val", &b);

    model.updateData("/val", "1");
    EXPECT_EQ(a.callCount, 1);
    EXPECT_EQ(b.callCount, 1);

    model.unbind("/val", &a);
    model.unbind("/val", &b);
}

TEST_F(DataModelTest, Bind_ParentPathChange_NotifiesChildObserver) {
    MockDataObserver observer;
    model.bind("/user/name", &observer);

    model.updateData("/user", R"({"name":"Charlie"})");
    EXPECT_GE(observer.callCount, 1);

    model.unbind("/user/name", &observer);
}

TEST_F(DataModelTest, Bind_ChildPathChange_NotifiesParentObserver) {
    MockDataObserver observer;
    model.bind("/user", &observer);

    model.updateData("/user/name", "\"Dave\"");
    EXPECT_GE(observer.callCount, 1);

    model.unbind("/user", &observer);
}

TEST_F(DataModelTest, Bind_UnrelatedPathChange_DoesNotNotify) {
    MockDataObserver observer;
    model.bind("/name", &observer);

    model.updateData("/age", "25");
    EXPECT_EQ(observer.callCount, 0);

    model.unbind("/name", &observer);
}

TEST_F(DataModelTest, Bind_NullObserver_Ignored) {
    EXPECT_NO_THROW(model.bind("/path", nullptr));
}

TEST_F(DataModelTest, Unbind_NullObserver_Ignored) {
    EXPECT_NO_THROW(model.unbind("/path", nullptr));
}

TEST_F(DataModelTest, Bind_EmptyPath_Ignored) {
    MockDataObserver observer;
    model.bind("", &observer);
    model.updateData("/val", "1");
    EXPECT_EQ(observer.callCount, 0);
}

TEST_F(DataModelTest, Unbind_NonBoundObserver_Ignored) {
    MockDataObserver observer;
    EXPECT_NO_THROW(model.unbind("/path", &observer));
}

TEST_F(DataModelTest, Unbind_NonExistentPath_Ignored) {
    MockDataObserver observer;
    EXPECT_NO_THROW(model.unbind("/never/bound", &observer));
}

TEST_F(DataModelTest, AppendData_NotifiesWithAppendMode) {
    MockDataObserver observer;
    model.bind("/data", &observer);

    model.appendData("/data", R"({"key":"val"})");
    EXPECT_EQ(observer.callCount, 1);
    EXPECT_TRUE(observer.lastAppendMode);

    model.unbind("/data", &observer);
}

TEST_F(DataModelTest, UpdateData_NotifiesWithNonAppendMode) {
    MockDataObserver observer;
    model.bind("/data", &observer);

    model.updateData("/data", "123");
    EXPECT_EQ(observer.callCount, 1);
    EXPECT_FALSE(observer.lastAppendMode);

    model.unbind("/data", &observer);
}

// ============================================================================
// syncBindingValue (delegates to updateData)
// ============================================================================

TEST_F(DataModelTest, SyncBindingValue_UpdatesData) {
    model.syncBindingValue("/sync", "\"synced\"");
    auto v = model.getValue("/sync");
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.dump(), "\"synced\"");
}

TEST_F(DataModelTest, SyncBindingValue_NotifiesObserver) {
    MockDataObserver observer;
    model.bind("/sync", &observer);

    model.syncBindingValue("/sync", "\"updated\"");
    EXPECT_EQ(observer.callCount, 1);

    model.unbind("/sync", &observer);
}

// ============================================================================
// Multiple sequential updates
// ============================================================================

TEST_F(DataModelTest, MultipleUpdates_AllReflectedInRoot) {
    model.updateData("/a", "1");
    model.updateData("/b", "2");
    model.updateData("/c", "3");
    auto root = model.getValue("/");
    EXPECT_TRUE(root.isValid());
}

TEST_F(DataModelTest, DuplicateBind_OnlyNotifiedOnce) {
    MockDataObserver observer;
    model.bind("/dup", &observer);
    model.bind("/dup", &observer);

    model.updateData("/dup", "1");
    EXPECT_EQ(observer.callCount, 1);

    model.unbind("/dup", &observer);
}
