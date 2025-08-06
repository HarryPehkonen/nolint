#include "file_io.hpp"
#include "nolint_app.hpp"
#include "terminal_io.hpp"
#include "warning_parser.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace nolint;
using ::testing::_;
using ::testing::Return;

// Mock FileSystem using Google Mock
class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(std::vector<std::string>, read_file, (const std::string& path), (override));
    MOCK_METHOD(bool, write_file, (const std::string& path, std::span<const std::string> lines),
                (override));
    MOCK_METHOD(bool, file_exists, (const std::string& path), (override));
};

// Mock WarningParser using Google Mock
class MockWarningParser : public IWarningParser {
public:
    MOCK_METHOD(std::vector<Warning>, parse_warnings, (std::istream & input), (override));
    MOCK_METHOD(std::vector<Warning>, parse_warnings, (std::string_view input), (override));
};

class NolintAppTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test warnings that represent different scenarios
        test_warnings = {Warning{"/src/main.cpp",
                                 42,
                                 10,
                                 "readability-magic-numbers",
                                 "magic number '42' should be replaced",
                                 {}},
                         Warning{"/src/utils.cpp",
                                 15,
                                 5,
                                 "readability-identifier-length",
                                 "variable name 'x' is too short",
                                 {}},
                         Warning{
                             "/src/large_function.cpp", 100, 1, "readability-function-size",
                             "function exceeds recommended size", 50 // function_lines
                         }};

        // Create test file content
        test_file_content = {"// Header comment", "void function() {",
                             "    int x = 42;  // Magic number here", "    process(x);", "}"};
    }

    std::vector<Warning> test_warnings;
    std::vector<std::string> test_file_content;
};

// Test the search/filter functionality directly through functional_core functions
// This demonstrates integration testing without the complex NolintApp setup
TEST_F(NolintAppTest, SearchFunctionality_FilterWarnings) {
    // Test the core filtering logic that NolintApp uses
    auto filtered = functional_core::filter_warnings(test_warnings, "magic");
    EXPECT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].warning_type, "readability-magic-numbers");

    auto filtered_by_file = functional_core::filter_warnings(test_warnings, "utils");
    EXPECT_EQ(filtered_by_file.size(), 1);
    EXPECT_EQ(filtered_by_file[0].file_path, "/src/utils.cpp");

    auto empty_filter = functional_core::filter_warnings(test_warnings, "nonexistent");
    EXPECT_EQ(empty_filter.size(), 0);

    // Test case-insensitive search
    auto case_insensitive = functional_core::filter_warnings(test_warnings, "MAGIC");
    EXPECT_EQ(case_insensitive.size(), 1);
}

// Test that demonstrates integration between warning parsing and processing
TEST_F(NolintAppTest, WarningProcessing_Integration) {
    // Test that function-size warnings are handled correctly
    std::vector<Warning> function_warnings = {Warning{
        "/src/big_function.cpp", 10, 1, "readability-function-size",
        "function exceeds recommended size", 75 // function_lines
    }};

    // Test that function-level warnings are correctly identified
    for (const auto& warning : function_warnings) {
        EXPECT_TRUE(functional_core::is_function_level_warning(warning.warning_type));
    }

    // Test that non-function warnings are correctly identified
    for (const auto& warning : test_warnings) {
        if (warning.warning_type != "readability-function-size") {
            EXPECT_FALSE(functional_core::is_function_level_warning(warning.warning_type));
        }
    }
}

// Test the modification creation and application logic
TEST_F(NolintAppTest, ModificationLogic_Integration) {
    // Test creating modifications for different styles
    for (const auto& warning : test_warnings) {
        auto specific_mod = functional_core::create_modification(
            warning, NolintStyle::NOLINT_SPECIFIC, test_file_content);
        EXPECT_EQ(specific_mod.style, NolintStyle::NOLINT_SPECIFIC);
        EXPECT_EQ(specific_mod.target_line, warning.line_number);

        auto nextline_mod = functional_core::create_modification(
            warning, NolintStyle::NOLINTNEXTLINE, test_file_content);
        EXPECT_EQ(nextline_mod.style, NolintStyle::NOLINTNEXTLINE);

        // For function-size warnings, test NOLINT_BLOCK
        if (functional_core::is_function_level_warning(warning.warning_type)) {
            auto block_mod = functional_core::create_modification(
                warning, NolintStyle::NOLINT_BLOCK, test_file_content);
            EXPECT_EQ(block_mod.style, NolintStyle::NOLINT_BLOCK);
        }
    }
}

// Test MockTerminal functionality directly
TEST_F(NolintAppTest, MockTerminal_Functionality) {
    MockTerminal terminal("test input\n");

    // Test basic output operations
    terminal.print("Hello ");
    terminal.print_line("World");

    std::string output = terminal.get_output();
    EXPECT_EQ(output, "Hello World\n");

    // Test input operations
    std::string line = terminal.read_line();
    EXPECT_EQ(line, "test input");

    // Test reset functionality
    terminal.reset_input("new input\n");
    std::string new_line = terminal.read_line();
    EXPECT_EQ(new_line, "new input");

    // Test interactive capability
    EXPECT_TRUE(terminal.is_interactive());
}

// Test that demonstrates the mock objects work correctly
TEST_F(NolintAppTest, MockObjects_BasicFunctionality) {
    // Test MockWarningParser
    MockWarningParser parser;
    std::istringstream input("test input");

    EXPECT_CALL(parser, parse_warnings(testing::An<std::istream&>()))
        .WillOnce(Return(test_warnings));

    auto result = parser.parse_warnings(input);
    EXPECT_EQ(result.size(), test_warnings.size());

    // Test MockFileSystem
    MockFileSystem filesystem;

    EXPECT_CALL(filesystem, read_file("/test/file.cpp")).WillOnce(Return(test_file_content));

    auto file_result = filesystem.read_file("/test/file.cpp");
    EXPECT_EQ(file_result.size(), test_file_content.size());

    EXPECT_CALL(filesystem, write_file("/test/file.cpp", _)).WillOnce(Return(true));

    bool write_result = filesystem.write_file("/test/file.cpp", test_file_content);
    EXPECT_TRUE(write_result);
}

// Demonstrate comprehensive testing approach without running the full app
TEST_F(NolintAppTest, ComprehensiveApproach_ComponentTesting) {
    // This test demonstrates how to test the components that NolintApp uses
    // without actually running the full interactive application

    // 1. Test warning parsing and filtering pipeline
    auto all_warnings = test_warnings;
    auto filtered_warnings = functional_core::filter_warnings(all_warnings, "magic");
    EXPECT_LT(filtered_warnings.size(), all_warnings.size());

    // 2. Test modification creation for filtered warnings
    EXPECT_EQ(filtered_warnings.size(), 1); // Should be 1 magic number warning

    // Create a warning that matches our test file content structure
    Warning realistic_warning{"/src/main.cpp",
                              3,
                              12,
                              "readability-magic-numbers",
                              "42 is a magic number; consider replacing it with a named constant",
                              {}};

    // Test modification creation
    auto mod = functional_core::create_modification(realistic_warning, NolintStyle::NOLINTNEXTLINE,
                                                    test_file_content);
    EXPECT_EQ(mod.style, NolintStyle::NOLINTNEXTLINE);
    EXPECT_EQ(mod.target_line, 3); // Should target line 3 where the magic number is

    // 3. Test that modifications can be applied to file content
    auto modified_lines = test_file_content;
    auto result = functional_core::apply_modification_to_lines(modified_lines, mod);
    modified_lines = std::move(result.lines);

    // Since we're using NOLINTNEXTLINE which adds a line before the target,
    // the total line count should increase by 1
    EXPECT_EQ(modified_lines.size(), test_file_content.size() + 1);

    // 4. Verify NOLINT comments were added
    bool found_nolint = false;
    for (const auto& line : modified_lines) {
        if (line.find("NOLINTNEXTLINE") != std::string::npos
            && line.find("readability-magic-numbers") != std::string::npos) {
            found_nolint = true;
            break;
        }
    }
    EXPECT_TRUE(found_nolint);

    // 5. Additional verification - test the filtering functionality works correctly
    EXPECT_EQ(filtered_warnings[0].warning_type, "readability-magic-numbers");
    EXPECT_EQ(filtered_warnings[0].line_number, 42); // From original test warning

    // 6. Test that function-level warnings are identified correctly
    for (const auto& warning : test_warnings) {
        if (warning.warning_type == "readability-function-size") {
            EXPECT_TRUE(functional_core::is_function_level_warning(warning.warning_type));
        } else {
            EXPECT_FALSE(functional_core::is_function_level_warning(warning.warning_type));
        }
    }
}