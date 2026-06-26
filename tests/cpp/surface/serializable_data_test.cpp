#include <gtest/gtest.h>
#include "surface/agenui_serializable_data.h"
#include "surface/agenui_serializable_data_impl.h"

using agenui::SerializableData;

// ============================================================================
// Construction & Null Safety
// ============================================================================

TEST(SerializableData, DefaultConstructed_IsNull) {
    SerializableData data;
    EXPECT_TRUE(data.isNull());
    EXPECT_FALSE(data.isValid());
}

TEST(SerializableData, DefaultConstructed_AccessorsReturnDefaults) {
    SerializableData data;
    EXPECT_EQ(data.asString("fallback"), "fallback");
    EXPECT_EQ(data.asInt(99), 99);
    EXPECT_EQ(data.asDouble(3.14), 3.14);
    EXPECT_EQ(data.asBool(true), true);
}

TEST(SerializableData, DefaultConstructed_ContainerOpsAreSafe) {
    SerializableData data;
    EXPECT_EQ(data.size(), 0u);
    EXPECT_TRUE(data.empty());
    EXPECT_FALSE(data.contains("any"));
    EXPECT_TRUE(data.getKeys().empty());
}

TEST(SerializableData, DefaultConstructed_SubscriptReturnsNull) {
    SerializableData data;
    auto child = data["key"];
    EXPECT_TRUE(child.isNull());
    auto child2 = data[0];
    EXPECT_TRUE(child2.isNull());
}

// ============================================================================
// Parse factory
// ============================================================================

TEST(SerializableData, Parse_ValidObject) {
    auto data = SerializableData::parse(R"({"name":"Alice","age":30})");
    EXPECT_TRUE(data.isValid());
    EXPECT_TRUE(data.isObject());
    EXPECT_EQ(data["name"].asString(), "Alice");
    EXPECT_EQ(data["age"].asInt(), 30);
}

TEST(SerializableData, Parse_ValidArray) {
    auto data = SerializableData::parse(R"([1,2,3])");
    EXPECT_TRUE(data.isArray());
    EXPECT_EQ(data.size(), 3u);
    EXPECT_EQ(data[0].asInt(), 1);
    EXPECT_EQ(data[2].asInt(), 3);
}

TEST(SerializableData, Parse_ValidString) {
    auto data = SerializableData::parse(R"("hello")");
    EXPECT_TRUE(data.isString());
    EXPECT_EQ(data.asString(), "hello");
}

TEST(SerializableData, Parse_ValidNumber) {
    auto data = SerializableData::parse("42.5");
    EXPECT_TRUE(data.isNumber());
    EXPECT_EQ(data.asDouble(), 42.5);
}

TEST(SerializableData, Parse_ValidBool) {
    auto data = SerializableData::parse("true");
    EXPECT_TRUE(data.isBool());
    EXPECT_EQ(data.asBool(), true);
}

TEST(SerializableData, Parse_InvalidJson_ReturnsNull) {
    auto data = SerializableData::parse("{{invalid");
    EXPECT_TRUE(data.isNull());
    EXPECT_FALSE(data.isValid());
}

TEST(SerializableData, Parse_EmptyString_ReturnsNull) {
    auto data = SerializableData::parse("");
    EXPECT_TRUE(data.isNull());
}

// ============================================================================
// Type checks
// ============================================================================

TEST(SerializableData, TypeCheck_String) {
    auto data = SerializableData::parse(R"("text")");
    EXPECT_TRUE(data.isString());
    EXPECT_FALSE(data.isNumber());
    EXPECT_FALSE(data.isBool());
    EXPECT_FALSE(data.isObject());
    EXPECT_FALSE(data.isArray());
}

TEST(SerializableData, TypeCheck_Object) {
    auto data = SerializableData::parse(R"({})");
    EXPECT_TRUE(data.isObject());
    EXPECT_FALSE(data.isString());
    EXPECT_FALSE(data.isArray());
}

TEST(SerializableData, TypeCheck_Null) {
    auto data = SerializableData::parse("null");
    EXPECT_TRUE(data.isNull());
    EXPECT_FALSE(data.isValid());
}

// ============================================================================
// Operator[] chaining (null-safe propagation)
// ============================================================================

TEST(SerializableData, NestedAccess_ValidPath) {
    auto data = SerializableData::parse(R"({"a":{"b":{"c":42}}})");
    EXPECT_EQ(data["a"]["b"]["c"].asInt(), 42);
}

TEST(SerializableData, NestedAccess_MissingKey_ReturnsNull) {
    auto data = SerializableData::parse(R"({"a":1})");
    auto miss = data["x"]["y"]["z"];
    EXPECT_TRUE(miss.isNull());
}

TEST(SerializableData, ArrayAccess_OutOfBounds_ReturnsNull) {
    auto data = SerializableData::parse("[10,20]");
    EXPECT_TRUE(data[5].isNull());
}

// ============================================================================
// Container operations
// ============================================================================

TEST(SerializableData, Contains_ExistingKey_True) {
    auto data = SerializableData::parse(R"({"x":1})");
    EXPECT_TRUE(data.contains("x"));
    EXPECT_FALSE(data.contains("y"));
}

TEST(SerializableData, Size_Object) {
    auto data = SerializableData::parse(R"({"a":1,"b":2,"c":3})");
    EXPECT_EQ(data.size(), 3u);
}

TEST(SerializableData, Size_Array) {
    auto data = SerializableData::parse("[1,2,3,4,5]");
    EXPECT_EQ(data.size(), 5u);
}

TEST(SerializableData, GetKeys_ReturnsAllKeys) {
    auto data = SerializableData::parse(R"({"name":"a","age":1})");
    auto keys = data.getKeys();
    EXPECT_EQ(keys.size(), 2u);
    // nlohmann::json keeps insertion order for objects
    EXPECT_EQ(keys[0], "age");  // alphabetical in nlohmann
    EXPECT_EQ(keys[1], "name");
}

// ============================================================================
// Serialization (dump)
// ============================================================================

TEST(SerializableData, Dump_Null_ReturnsNullString) {
    SerializableData data;
    EXPECT_EQ(data.dump(), "null");
}

TEST(SerializableData, Dump_Number) {
    auto data = SerializableData::parse("123");
    EXPECT_EQ(data.dump(), "123");
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(SerializableData, Equality_BothNull_True) {
    SerializableData a, b;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(SerializableData, Equality_SameValue_True) {
    auto a = SerializableData::parse(R"({"x":1})");
    auto b = SerializableData::parse(R"({"x":1})");
    EXPECT_TRUE(a == b);
}

TEST(SerializableData, Equality_DifferentValue_False) {
    auto a = SerializableData::parse(R"({"x":1})");
    auto b = SerializableData::parse(R"({"x":2})");
    EXPECT_TRUE(a != b);
}

TEST(SerializableData, Equality_NullVsValid_False) {
    SerializableData a;
    auto b = SerializableData::parse("1");
    EXPECT_TRUE(a != b);
}

// ============================================================================
// Copy & Move semantics
// ============================================================================

TEST(SerializableData, Copy_SharesData) {
    auto orig = SerializableData::parse(R"({"v":99})");
    SerializableData copy = orig;
    EXPECT_EQ(copy["v"].asInt(), 99);
    EXPECT_TRUE(orig == copy);
}

TEST(SerializableData, Move_TransfersOwnership) {
    auto orig = SerializableData::parse(R"({"v":99})");
    SerializableData moved = std::move(orig);
    EXPECT_EQ(moved["v"].asInt(), 99);
    EXPECT_TRUE(orig.isNull());  // NOLINT(bugprone-use-after-move)
}

// ============================================================================
// Iterator
// ============================================================================

TEST(SerializableData, Iterator_Object_VisitsAllEntries) {
    auto data = SerializableData::parse(R"({"a":1,"b":2})");
    int count = 0;
    for (auto it = data.begin(); it != data.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST(SerializableData, Iterator_Array_VisitsAllElements) {
    auto data = SerializableData::parse("[10,20,30]");
    int count = 0;
    for (auto it = data.begin(); it != data.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST(SerializableData, Iterator_Null_BeginEqualsEnd) {
    SerializableData data;
    EXPECT_TRUE(data.begin() == data.end());
}

TEST(SerializableData, Iterator_Key_Object) {
    auto data = SerializableData::parse(R"({"hello":"world"})");
    auto it = data.begin();
    EXPECT_EQ(it.key(), "hello");
    EXPECT_EQ((*it).asString(), "world");
}

TEST(SerializableData, Iterator_Key_Array_ReturnsIndex) {
    auto data = SerializableData::parse("[100,200]");
    auto it = data.begin();
    EXPECT_EQ(it.key(), "0");
    ++it;
    EXPECT_EQ(it.key(), "1");
}
