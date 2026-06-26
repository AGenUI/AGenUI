#include <gtest/gtest.h>
#include "surface/token_parser/agenui_token_parser.h"

using agenui::TokenParser;
using agenui::ThemeMode;

// ============================================================================
// TokenParser singleton tests
//
// The singleton loads the built-in design token config on first access.
// These tests exercise resolve(), loadFromJsonString(), setThemeMode().
// ============================================================================

class TokenParserTest : public ::testing::Test {
protected:
    TokenParser& parser = TokenParser::getInstance();

    void SetUp() override {
        parser.setThemeMode(ThemeMode::Light);
    }
};

// ============================================================================
// resolve() — unknown tokens
// ============================================================================

TEST_F(TokenParserTest, Resolve_UnknownToken_ReturnsSameName) {
    EXPECT_EQ(parser.resolve("nonexistent.token.name"), "nonexistent.token.name");
}

TEST_F(TokenParserTest, Resolve_EmptyString_ReturnsEmptyString) {
    EXPECT_EQ(parser.resolve(""), "");
}

// ============================================================================
// loadFromJsonString()
// ============================================================================

TEST_F(TokenParserTest, LoadFromJsonString_ValidJson_ReturnsTrue) {
    std::string json = R"({
        "designTokens": {
            "test.color.primary": {
                "type": "color",
                "light": "#FF0000",
                "dark": "#00FF00"
            }
        }
    })";
    EXPECT_TRUE(parser.loadFromJsonString(json));
}

TEST_F(TokenParserTest, LoadFromJsonString_InvalidJson_ReturnsFalse) {
    EXPECT_FALSE(parser.loadFromJsonString("{invalid json"));
}

TEST_F(TokenParserTest, LoadFromJsonString_MissingDesignTokens_ReturnsFalse) {
    EXPECT_FALSE(parser.loadFromJsonString(R"({"other": {}})"));
}

TEST_F(TokenParserTest, LoadFromJsonString_EmptyString_ReturnsFalse) {
    EXPECT_FALSE(parser.loadFromJsonString(""));
}

TEST_F(TokenParserTest, LoadFromJsonString_ArrayRoot_ReturnsFalse) {
    EXPECT_FALSE(parser.loadFromJsonString("[1, 2, 3]"));
}

TEST_F(TokenParserTest, LoadFromJsonString_DesignTokensNotObject_ReturnsFalse) {
    EXPECT_FALSE(parser.loadFromJsonString(R"({"designTokens": "not_object"})"));
}

// ============================================================================
// resolve() — after loading tokens
// ============================================================================

TEST_F(TokenParserTest, Resolve_LightMode_ReturnsLightValue) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.color": {
                "type": "color",
                "light": "#AAAAAA",
                "dark": "#BBBBBB"
            }
        }
    })");
    parser.setThemeMode(ThemeMode::Light);
    EXPECT_EQ(parser.resolve("tp.test.color"), "#AAAAAA");
}

TEST_F(TokenParserTest, Resolve_DarkMode_ReturnsDarkValue) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.color2": {
                "type": "color",
                "light": "#AAAAAA",
                "dark": "#BBBBBB"
            }
        }
    })");
    parser.setThemeMode(ThemeMode::Dark);
    EXPECT_EQ(parser.resolve("tp.test.color2"), "#BBBBBB");
}

TEST_F(TokenParserTest, Resolve_DarkMode_NoDarkValue_FallsBackToLight) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.lightonly": {
                "type": "color",
                "light": "#CCCCCC"
            }
        }
    })");
    parser.setThemeMode(ThemeMode::Dark);
    EXPECT_EQ(parser.resolve("tp.test.lightonly"), "#CCCCCC");
}

TEST_F(TokenParserTest, Resolve_LightMode_NoLightValue_FallsBackToDark) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.darkonly": {
                "type": "color",
                "dark": "#DDDDDD"
            }
        }
    })");
    parser.setThemeMode(ThemeMode::Light);
    EXPECT_EQ(parser.resolve("tp.test.darkonly"), "#DDDDDD");
}

// ============================================================================
// setThemeMode()
// ============================================================================

TEST_F(TokenParserTest, SetThemeMode_SameMode_ReturnsFalse) {
    parser.setThemeMode(ThemeMode::Light);
    EXPECT_FALSE(parser.setThemeMode(ThemeMode::Light));
}

TEST_F(TokenParserTest, SetThemeMode_DifferentMode_ReturnsTrue) {
    parser.setThemeMode(ThemeMode::Light);
    EXPECT_TRUE(parser.setThemeMode(ThemeMode::Dark));
}

TEST_F(TokenParserTest, SetThemeMode_Toggle_ChangesResolvedValues) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.toggle": {
                "type": "color",
                "light": "#111111",
                "dark": "#222222"
            }
        }
    })");
    parser.setThemeMode(ThemeMode::Light);
    EXPECT_EQ(parser.resolve("tp.test.toggle"), "#111111");

    parser.setThemeMode(ThemeMode::Dark);
    EXPECT_EQ(parser.resolve("tp.test.toggle"), "#222222");
}

// ============================================================================
// loadFromJsonString() — edge cases
// ============================================================================

TEST_F(TokenParserTest, LoadFromJsonString_SkipsNonObjectTokens) {
    std::string json = R"({
        "designTokens": {
            "valid.token": {
                "type": "color",
                "light": "#EEEEEE"
            },
            "invalid.token": "just_a_string",
            "invalid.token2": 42
        }
    })";
    EXPECT_TRUE(parser.loadFromJsonString(json));
    EXPECT_EQ(parser.resolve("valid.token"), "#EEEEEE");
}

TEST_F(TokenParserTest, LoadFromJsonString_EmptyDesignTokens_ReturnsTrue) {
    EXPECT_TRUE(parser.loadFromJsonString(R"({"designTokens": {}})"));
}

TEST_F(TokenParserTest, LoadFromJsonString_TokenWithEmptyLightAndDark_NotStored) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.empty": {
                "type": "color"
            }
        }
    })");
    EXPECT_EQ(parser.resolve("tp.test.empty"), "tp.test.empty");
}

TEST_F(TokenParserTest, LoadFromJsonString_MultipleTokens) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.a": {"type": "color", "light": "#AAA"},
            "tp.test.b": {"type": "color", "light": "#BBB"},
            "tp.test.c": {"type": "color", "light": "#CCC"}
        }
    })");
    EXPECT_EQ(parser.resolve("tp.test.a"), "#AAA");
    EXPECT_EQ(parser.resolve("tp.test.b"), "#BBB");
    EXPECT_EQ(parser.resolve("tp.test.c"), "#CCC");
}

TEST_F(TokenParserTest, LoadFromJsonString_OverwritesExistingToken) {
    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.overwrite": {"type": "color", "light": "#111"}
        }
    })");
    EXPECT_EQ(parser.resolve("tp.test.overwrite"), "#111");

    parser.loadFromJsonString(R"({
        "designTokens": {
            "tp.test.overwrite": {"type": "color", "light": "#222"}
        }
    })");
    EXPECT_EQ(parser.resolve("tp.test.overwrite"), "#222");
}
