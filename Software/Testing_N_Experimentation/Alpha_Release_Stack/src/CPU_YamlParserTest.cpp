/*****************************************************************
 * @file CPU_YamlParserTest.cpp
 * @brief YAML Parser Unit Tests
 * 
 * Tests the YamlParser implementation:
 * - Parsing scalars (string, int, float, bool)
 * - Parsing nested maps
 * - Parsing arrays
 * - Serializing back to YAML
 * 
 * Build: pio run -e CPU_YamlParserTest
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "Drivers/YamlParser.hpp"

using namespace Drivers;

static const char* TAG = "YAML_TEST";

//=============================================================================
// Test Helpers
//=============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    printf("\n[TEST] %s\n", name); \
    printf("────────────────────────────────────────\n");

#define ASSERT_EQ(actual, expected, msg) \
    if ((actual) == (expected)) { \
        printf("  ✓ %s\n", msg); \
        g_passed++; \
    } else { \
        printf("  ✗ %s (expected: %s, got something else)\n", msg, #expected); \
        g_failed++; \
    }

#define ASSERT_STR_EQ(actual, expected, msg) \
    if (std::string(actual) == std::string(expected)) { \
        printf("  ✓ %s = \"%s\"\n", msg, expected); \
        g_passed++; \
    } else { \
        printf("  ✗ %s (expected: \"%s\", got: \"%s\")\n", msg, expected, (actual).c_str()); \
        g_failed++; \
    }

#define ASSERT_INT_EQ(actual, expected, msg) \
    if ((actual) == (expected)) { \
        printf("  ✓ %s = %d\n", msg, expected); \
        g_passed++; \
    } else { \
        printf("  ✗ %s (expected: %d, got: %d)\n", msg, expected, actual); \
        g_failed++; \
    }

#define ASSERT_FLOAT_EQ(actual, expected, msg) \
    if (std::abs((actual) - (expected)) < 0.001f) { \
        printf("  ✓ %s = %.3f\n", msg, expected); \
        g_passed++; \
    } else { \
        printf("  ✗ %s (expected: %.3f, got: %.3f)\n", msg, expected, actual); \
        g_failed++; \
    }

#define ASSERT_BOOL_EQ(actual, expected, msg) \
    if ((actual) == (expected)) { \
        printf("  ✓ %s = %s\n", msg, expected ? "true" : "false"); \
        g_passed++; \
    } else { \
        printf("  ✗ %s (expected: %s, got: %s)\n", msg, \
               expected ? "true" : "false", actual ? "true" : "false"); \
        g_failed++; \
    }

//=============================================================================
// Test Cases
//=============================================================================

void testSimpleScalars() {
    TEST("Simple Scalars");
    
    const char* yaml = R"(
name: TestScene
id: 42
version: 1.5
enabled: true
disabled: false
empty_value:
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_STR_EQ(root["name"].asString(), "TestScene", "name");
    ASSERT_INT_EQ(root["id"].asInt(), 42, "id");
    ASSERT_FLOAT_EQ(root["version"].asFloat(), 1.5f, "version");
    ASSERT_BOOL_EQ(root["enabled"].asBool(), true, "enabled");
    ASSERT_BOOL_EQ(root["disabled"].asBool(), false, "disabled");
    ASSERT_EQ(root["empty_value"].isNull() || root["empty_value"].asString().empty(), true, "empty_value is null/empty");
}

void testNestedMaps() {
    TEST("Nested Maps");
    
    const char* yaml = R"(
animation:
  type: gyro_eyes
  spriteId: 5
  sensitivity: 1.5
  bgColor:
    r: 255
    g: 128
    b: 0
display:
  enabled: true
  brightness: 80
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_STR_EQ(root["animation"]["type"].asString(), "gyro_eyes", "animation.type");
    ASSERT_INT_EQ(root["animation"]["spriteId"].asInt(), 5, "animation.spriteId");
    ASSERT_FLOAT_EQ(root["animation"]["sensitivity"].asFloat(), 1.5f, "animation.sensitivity");
    ASSERT_INT_EQ(root["animation"]["bgColor"]["r"].asInt(), 255, "animation.bgColor.r");
    ASSERT_INT_EQ(root["animation"]["bgColor"]["g"].asInt(), 128, "animation.bgColor.g");
    ASSERT_INT_EQ(root["animation"]["bgColor"]["b"].asInt(), 0, "animation.bgColor.b");
    ASSERT_BOOL_EQ(root["display"]["enabled"].asBool(), true, "display.enabled");
    ASSERT_INT_EQ(root["display"]["brightness"].asInt(), 80, "display.brightness");
}

void testSimpleArrays() {
    TEST("Simple Arrays");
    
    const char* yaml = R"(
colors:
  - red
  - green
  - blue
numbers:
  - 1
  - 2
  - 3
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_EQ(root["colors"].isArray(), true, "colors is array");
    ASSERT_INT_EQ((int)root["colors"].size(), 3, "colors.size");
    ASSERT_STR_EQ(root["colors"][0].asString(), "red", "colors[0]");
    ASSERT_STR_EQ(root["colors"][1].asString(), "green", "colors[1]");
    ASSERT_STR_EQ(root["colors"][2].asString(), "blue", "colors[2]");
    
    ASSERT_INT_EQ((int)root["numbers"].size(), 3, "numbers.size");
    ASSERT_INT_EQ(root["numbers"][0].asInt(), 1, "numbers[0]");
    ASSERT_INT_EQ(root["numbers"][1].asInt(), 2, "numbers[1]");
    ASSERT_INT_EQ(root["numbers"][2].asInt(), 3, "numbers[2]");
}

void testArrayOfMaps() {
    TEST("Array of Maps (Sprites)");
    
    const char* yaml = R"(
sprites:
  - name: eye_left
    id: 1
    width: 32
    height: 32
  - name: eye_right
    id: 2
    width: 32
    height: 32
  - name: pupil
    id: 3
    width: 16
    height: 16
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_EQ(root["sprites"].isArray(), true, "sprites is array");
    ASSERT_INT_EQ((int)root["sprites"].size(), 3, "sprites.size");
    
    ASSERT_STR_EQ(root["sprites"][0]["name"].asString(), "eye_left", "sprites[0].name");
    ASSERT_INT_EQ(root["sprites"][0]["id"].asInt(), 1, "sprites[0].id");
    ASSERT_INT_EQ(root["sprites"][0]["width"].asInt(), 32, "sprites[0].width");
    
    ASSERT_STR_EQ(root["sprites"][1]["name"].asString(), "eye_right", "sprites[1].name");
    ASSERT_INT_EQ(root["sprites"][1]["id"].asInt(), 2, "sprites[1].id");
    
    ASSERT_STR_EQ(root["sprites"][2]["name"].asString(), "pupil", "sprites[2].name");
    ASSERT_INT_EQ(root["sprites"][2]["width"].asInt(), 16, "sprites[2].width");
}

void testComments() {
    TEST("Comments");
    
    const char* yaml = R"(
# This is a comment
name: MyScene  # inline comment
# Another comment
id: 100

# Comment before section
animation:
  type: static  # type comment
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_STR_EQ(root["name"].asString(), "MyScene", "name (comments stripped)");
    ASSERT_INT_EQ(root["id"].asInt(), 100, "id");
    ASSERT_STR_EQ(root["animation"]["type"].asString(), "static", "animation.type");
}

void testQuotedStrings() {
    TEST("Quoted Strings");
    
    const char* yaml = R"(
single_quoted: 'Hello World'
double_quoted: "Hello World"
with_colon: "value: with colon"
with_hash: "value # with hash"
unquoted: Hello World
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_STR_EQ(root["single_quoted"].asString(), "Hello World", "single_quoted");
    ASSERT_STR_EQ(root["double_quoted"].asString(), "Hello World", "double_quoted");
    ASSERT_STR_EQ(root["with_colon"].asString(), "value: with colon", "with_colon");
    ASSERT_STR_EQ(root["with_hash"].asString(), "value # with hash", "with_hash");
    ASSERT_STR_EQ(root["unquoted"].asString(), "Hello World", "unquoted");
}

void testBooleanVariants() {
    TEST("Boolean Variants");
    
    const char* yaml = R"(
bool_true: true
bool_false: false
bool_yes: yes
bool_no: no
bool_on: on
bool_off: off
bool_1: 1
bool_0: 0
bool_TRUE: TRUE
bool_FALSE: FALSE
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_BOOL_EQ(root["bool_true"].asBool(), true, "true");
    ASSERT_BOOL_EQ(root["bool_false"].asBool(), false, "false");
    ASSERT_BOOL_EQ(root["bool_yes"].asBool(), true, "yes");
    ASSERT_BOOL_EQ(root["bool_no"].asBool(), false, "no");
    ASSERT_BOOL_EQ(root["bool_on"].asBool(), true, "on");
    ASSERT_BOOL_EQ(root["bool_off"].asBool(), false, "off");
    ASSERT_BOOL_EQ(root["bool_1"].asBool(), true, "1");
    ASSERT_BOOL_EQ(root["bool_0"].asBool(), false, "0");
    ASSERT_BOOL_EQ(root["bool_TRUE"].asBool(), true, "TRUE");
    ASSERT_BOOL_EQ(root["bool_FALSE"].asBool(), false, "FALSE");
}

void testDefaultValues() {
    TEST("Default Values for Missing Keys");
    
    const char* yaml = R"(
existing_key: value
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_STR_EQ(root["missing_key"].asString("default"), "default", "missing string default");
    ASSERT_INT_EQ(root["missing_key"].asInt(42), 42, "missing int default");
    ASSERT_FLOAT_EQ(root["missing_key"].asFloat(3.14f), 3.14f, "missing float default");
    ASSERT_BOOL_EQ(root["missing_key"].asBool(true), true, "missing bool default");
}

void testSerialization() {
    TEST("Serialization (Round-Trip)");
    
    // Create a node programmatically
    YamlNode root;
    root["name"] = YamlNode("TestScene");
    root["id"] = YamlNode(42);
    root["enabled"] = YamlNode(true);
    
    root["animation"].makeMap();
    root["animation"]["type"] = YamlNode("gyro_eyes");
    root["animation"]["speed"] = YamlNode(1.5f);
    
    root["colors"].makeArray();
    root["colors"].push(YamlNode("red"));
    root["colors"].push(YamlNode("green"));
    root["colors"].push(YamlNode("blue"));
    
    // Serialize
    std::string yaml = YamlParser::serialize(root);
    printf("  Serialized YAML:\n");
    printf("  ────────────────\n");
    // Print with indent
    size_t start = 0;
    size_t end;
    while ((end = yaml.find('\n', start)) != std::string::npos) {
        printf("  %s\n", yaml.substr(start, end - start).c_str());
        start = end + 1;
    }
    if (start < yaml.length()) {
        printf("  %s\n", yaml.substr(start).c_str());
    }
    
    // Parse the serialized YAML
    YamlNode parsed = YamlParser::parse(yaml);
    
    // Verify values
    ASSERT_STR_EQ(parsed["name"].asString(), "TestScene", "round-trip name");
    ASSERT_INT_EQ(parsed["id"].asInt(), 42, "round-trip id");
    ASSERT_BOOL_EQ(parsed["enabled"].asBool(), true, "round-trip enabled");
    ASSERT_STR_EQ(parsed["animation"]["type"].asString(), "gyro_eyes", "round-trip animation.type");
    ASSERT_INT_EQ((int)parsed["colors"].size(), 3, "round-trip colors.size");
    ASSERT_STR_EQ(parsed["colors"][0].asString(), "red", "round-trip colors[0]");
}

void testCompleteSceneYaml() {
    TEST("Complete Scene YAML (Real-World)");
    
    const char* yaml = R"(
# Scene Configuration
name: GyroEyesScene
id: 1
version: 1.0

# Animation settings
animation:
  type: gyro_eyes
  spriteId: 1
  posX: 64
  posY: 16
  sensitivity: 1.5
  mirror: true
  bgColor:
    r: 0
    g: 0
    b: 0

# Display settings
displayEnabled: true
ledsEnabled: false

# LED configuration
leds:
  brightness: 80
  color:
    r: 255
    g: 128
    b: 0

# Sprite definitions
sprites:
  - name: eye_sprite
    id: 1
    path: /sprites/eye_32x32.bin
    width: 32
    height: 32
  - name: pupil_sprite
    id: 2
    path: /sprites/pupil_16x16.bin
    width: 16
    height: 16
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    // Top-level
    ASSERT_STR_EQ(root["name"].asString(), "GyroEyesScene", "name");
    ASSERT_INT_EQ(root["id"].asInt(), 1, "id");
    ASSERT_FLOAT_EQ(root["version"].asFloat(), 1.0f, "version");
    
    // Animation
    ASSERT_STR_EQ(root["animation"]["type"].asString(), "gyro_eyes", "animation.type");
    ASSERT_INT_EQ(root["animation"]["spriteId"].asInt(), 1, "animation.spriteId");
    ASSERT_INT_EQ(root["animation"]["posX"].asInt(), 64, "animation.posX");
    ASSERT_FLOAT_EQ(root["animation"]["sensitivity"].asFloat(), 1.5f, "animation.sensitivity");
    ASSERT_BOOL_EQ(root["animation"]["mirror"].asBool(), true, "animation.mirror");
    ASSERT_INT_EQ(root["animation"]["bgColor"]["r"].asInt(), 0, "animation.bgColor.r");
    
    // Display/LEDs
    ASSERT_BOOL_EQ(root["displayEnabled"].asBool(), true, "displayEnabled");
    ASSERT_BOOL_EQ(root["ledsEnabled"].asBool(), false, "ledsEnabled");
    ASSERT_INT_EQ(root["leds"]["brightness"].asInt(), 80, "leds.brightness");
    ASSERT_INT_EQ(root["leds"]["color"]["r"].asInt(), 255, "leds.color.r");
    
    // Sprites array
    ASSERT_INT_EQ((int)root["sprites"].size(), 2, "sprites.size");
    ASSERT_STR_EQ(root["sprites"][0]["name"].asString(), "eye_sprite", "sprites[0].name");
    ASSERT_STR_EQ(root["sprites"][0]["path"].asString(), "/sprites/eye_32x32.bin", "sprites[0].path");
    ASSERT_INT_EQ(root["sprites"][0]["width"].asInt(), 32, "sprites[0].width");
    ASSERT_STR_EQ(root["sprites"][1]["name"].asString(), "pupil_sprite", "sprites[1].name");
    ASSERT_INT_EQ(root["sprites"][1]["width"].asInt(), 16, "sprites[1].width");
}

void testHasKeyAndKeys() {
    TEST("hasKey() and keys()");
    
    const char* yaml = R"(
name: Test
id: 1
nested:
  a: 1
  b: 2
  c: 3
)";
    
    YamlNode root = YamlParser::parse(yaml);
    
    ASSERT_BOOL_EQ(root.hasKey("name"), true, "hasKey(name)");
    ASSERT_BOOL_EQ(root.hasKey("id"), true, "hasKey(id)");
    ASSERT_BOOL_EQ(root.hasKey("missing"), false, "hasKey(missing)");
    ASSERT_BOOL_EQ(root.hasKey("nested"), true, "hasKey(nested)");
    
    auto nestedKeys = root["nested"].keys();
    ASSERT_INT_EQ((int)nestedKeys.size(), 3, "nested.keys().size");
    
    printf("  Nested keys: ");
    for (const auto& k : nestedKeys) {
        printf("%s ", k.c_str());
    }
    printf("\n");
    g_passed++;
}

//=============================================================================
// Main
//=============================================================================

extern "C" void app_main() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                   YAML PARSER TEST SUITE                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Run all tests
    testSimpleScalars();
    testNestedMaps();
    testSimpleArrays();
    testArrayOfMaps();
    testComments();
    testQuotedStrings();
    testBooleanVariants();
    testDefaultValues();
    testSerialization();
    testCompleteSceneYaml();
    testHasKeyAndKeys();
    
    // Summary
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                      TEST SUMMARY                             ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Passed: %-4d                                                 ║\n", g_passed);
    printf("║  Failed: %-4d                                                 ║\n", g_failed);
    printf("║  Total:  %-4d                                                 ║\n", g_passed + g_failed);
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    if (g_failed == 0) {
        printf("\n  ✓ ALL TESTS PASSED!\n\n");
    } else {
        printf("\n  ✗ SOME TESTS FAILED\n\n");
    }
    
    // Keep alive
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
