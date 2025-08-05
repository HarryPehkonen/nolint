#include "functional_core.hpp"
#include "warning_parser.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace nolint;
using namespace nolint::functional_core;

class WarningParserTest : public ::testing::Test {
protected:
    // Sample clang-tidy output for testing
    const std::string sample_clang_tidy_output = R"(
/home/user/project/src/main.cpp:42:5: warning: 42 is a magic number; consider replacing it with a named constant [readability-magic-numbers]
    int value = 42;
    ^~

/home/user/project/src/parser.cpp:78:1: warning: function 'parse_expression' exceeds recommended size/complexity thresholds [readability-function-size]
void Parser::parse_expression(const Token* tokens, size_t count) {
^
/home/user/project/src/parser.cpp:78:1: note: 44 lines including whitespace and comments (threshold 30)

/home/user/project/include/utils.hpp:123:15: warning: variable name 'x' is too short [readability-identifier-naming]
    int x = getValue();
              ^~~~~~~~~

/home/user/project/src/helper.cpp:156:20: warning: avoid unnecessary copy on every iteration; consider using a reference instead [performance-for-range-copy]
    for (auto item : container) {
                   ^
)";

    const std::string malformed_clang_tidy_output = R"(
This is not a warning line
/invalid/path:not-a-number:45: warning: test [warning-type]
/path/file.cpp:123:abc: warning: invalid column [warning-type]
random text with no structure
: warning: missing file path [warning-type]
)";
};

// Test pure functional core functions first

TEST_F(WarningParserTest, ExtractWarningComponents_ValidMagicNumber) {
    std::string_view line
        = "/home/user/src/main.cpp:42:5: warning: 42 is a magic number [readability-magic-numbers]";

    auto result = extract_warning_components(line);

    ASSERT_TRUE(result.has_value());
    auto [file_path, line_num, column, message, warning_type] = *result;

    EXPECT_EQ(file_path, "/home/user/src/main.cpp");
    EXPECT_EQ(line_num, 42);
    EXPECT_EQ(column, 5);
    EXPECT_EQ(message, "42 is a magic number");
    EXPECT_EQ(warning_type, "readability-magic-numbers");
}

TEST_F(WarningParserTest, ExtractWarningComponents_ValidFunctionSize) {
    std::string_view line
        = "/src/parser.cpp:78:1: warning: function exceeds size [readability-function-size]";

    auto result = extract_warning_components(line);

    ASSERT_TRUE(result.has_value());
    auto [file_path, line_num, column, message, warning_type] = *result;

    EXPECT_EQ(file_path, "/src/parser.cpp");
    EXPECT_EQ(line_num, 78);
    EXPECT_EQ(column, 1);
    EXPECT_EQ(message, "function exceeds size");
    EXPECT_EQ(warning_type, "readability-function-size");
}

TEST_F(WarningParserTest, ExtractWarningComponents_RelativePath) {
    std::string_view line = "./src/main.cpp:10:3: warning: test message [test-warning]";

    auto result = extract_warning_components(line);

    ASSERT_TRUE(result.has_value());
    auto [file_path, line_num, column, message, warning_type] = *result;

    EXPECT_EQ(file_path, "./src/main.cpp");
    EXPECT_EQ(line_num, 10);
    EXPECT_EQ(column, 3);
}

TEST_F(WarningParserTest, ExtractWarningComponents_ComplexMessage) {
    std::string_view line = "/path/file.cpp:1:2: warning: very long message with: colons and "
                            "[brackets] inside [warning-type]";

    auto result = extract_warning_components(line);

    ASSERT_TRUE(result.has_value());
    auto [file_path, line_num, column, message, warning_type] = *result;

    EXPECT_EQ(message, "very long message with: colons and [brackets] inside");
    EXPECT_EQ(warning_type, "warning-type");
}

TEST_F(WarningParserTest, ExtractWarningComponents_InvalidLines) {
    // Non-warning lines
    EXPECT_FALSE(extract_warning_components("This is just text").has_value());
    EXPECT_FALSE(extract_warning_components("").has_value());
    EXPECT_FALSE(extract_warning_components("file.cpp:123: note: this is a note").has_value());

    // Malformed warning lines
    EXPECT_FALSE(
        extract_warning_components("/path:abc:5: warning: invalid line number [type]").has_value());
    EXPECT_FALSE(
        extract_warning_components("/path:5:xyz: warning: invalid column [type]").has_value());
    EXPECT_FALSE(extract_warning_components("/path:5:3: error: this is an error [type]")
                     .has_value()); // not warning
}

TEST_F(WarningParserTest, ExtractFunctionLines_ValidNote) {
    std::string_view line
        = "/path/file.cpp:78:1: note: 44 lines including whitespace and comments (threshold 30)";

    auto result = extract_function_lines(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 44);
}

TEST_F(WarningParserTest, ExtractFunctionLines_VariousFormats) {
    EXPECT_EQ(extract_function_lines("note: 25 lines including whitespace"), 25);
    EXPECT_EQ(extract_function_lines("  note: 100 lines including comments"), 100);
    EXPECT_EQ(extract_function_lines("note: 1 line including whitespace"), 1);
    EXPECT_EQ(extract_function_lines("    note: 999 lines BLAH BLAH"), 999);
}

TEST_F(WarningParserTest, ExtractFunctionLines_InvalidNotes) {
    EXPECT_FALSE(extract_function_lines("This is not a note").has_value());
    EXPECT_FALSE(extract_function_lines("warning: 44 lines").has_value()); // not note
    EXPECT_FALSE(extract_function_lines("note: no number here").has_value());
    EXPECT_FALSE(extract_function_lines("note: abc lines").has_value()); // invalid number
    EXPECT_FALSE(extract_function_lines("").has_value());
}

TEST_F(WarningParserTest, IsFunctionLevelWarning_FunctionTypes) {
    EXPECT_TRUE(is_function_level_warning("readability-function-size"));
    EXPECT_TRUE(is_function_level_warning("readability-function-cognitive-complexity"));
    EXPECT_TRUE(is_function_level_warning("prefix-function-size-suffix"));
}

TEST_F(WarningParserTest, IsFunctionLevelWarning_NonFunctionTypes) {
    EXPECT_FALSE(is_function_level_warning("readability-magic-numbers"));
    EXPECT_FALSE(is_function_level_warning("readability-identifier-naming"));
    EXPECT_FALSE(is_function_level_warning("performance-for-range-copy"));
    EXPECT_FALSE(is_function_level_warning("cppcoreguidelines-avoid-goto"));
}

TEST_F(WarningParserTest, NormalizeFilePath_RemoveLeadingDot) {
    EXPECT_EQ(normalize_file_path("./src/main.cpp"), "src/main.cpp");
    EXPECT_EQ(normalize_file_path("./include/header.hpp"), "include/header.hpp");
}

TEST_F(WarningParserTest, NormalizeFilePath_KeepAbsolutePaths) {
    // All absolute paths should remain absolute - clang-tidy gives us correct paths
    EXPECT_EQ(normalize_file_path("/usr/include/stdio.h"), "/usr/include/stdio.h");
    EXPECT_EQ(normalize_file_path("/opt/local/include/header.h"), "/opt/local/include/header.h");
    EXPECT_EQ(normalize_file_path("/home/user/project/src/main.cpp"),
              "/home/user/project/src/main.cpp");
    EXPECT_EQ(normalize_file_path("/workspace/myproject/include/header.hpp"),
              "/workspace/myproject/include/header.hpp");
}

TEST_F(WarningParserTest, NormalizeFilePath_KeepRelativeWithoutDot) {
    EXPECT_EQ(normalize_file_path("src/main.cpp"), "src/main.cpp");
    EXPECT_EQ(normalize_file_path("../include/header.hpp"), "../include/header.hpp");
}

// Test the concrete parser class

TEST_F(WarningParserTest, ClangTidyParser_ParseSingleWarning) {
    ClangTidyParser parser;
    std::string input
        = "/src/main.cpp:42:5: warning: 42 is a magic number [readability-magic-numbers]\n";

    auto warnings = parser.parse_warnings(input);

    ASSERT_EQ(warnings.size(), 1);
    const auto& warning = warnings[0];

    EXPECT_EQ(warning.file_path, "/src/main.cpp"); // absolute path preserved
    EXPECT_EQ(warning.line_number, 42);
    EXPECT_EQ(warning.column_number, 5);
    EXPECT_EQ(warning.warning_type, "readability-magic-numbers");
    EXPECT_EQ(warning.message, "42 is a magic number");
    EXPECT_FALSE(warning.function_lines.has_value());
}

TEST_F(WarningParserTest, ClangTidyParser_ParseFunctionSizeWithNote) {
    ClangTidyParser parser;
    std::string input
        = R"(/src/parser.cpp:78:1: warning: function exceeds size [readability-function-size]
/src/parser.cpp:78:1: note: 44 lines including whitespace and comments
)";

    auto warnings = parser.parse_warnings(input);

    ASSERT_EQ(warnings.size(), 1);
    const auto& warning = warnings[0];

    EXPECT_EQ(warning.file_path, "/src/parser.cpp");
    EXPECT_EQ(warning.line_number, 78);
    EXPECT_EQ(warning.warning_type, "readability-function-size");
    ASSERT_TRUE(warning.function_lines.has_value());
    EXPECT_EQ(warning.function_lines.value(), 44);
}

TEST_F(WarningParserTest, ClangTidyParser_ParseMultipleWarnings) {
    ClangTidyParser parser;

    auto warnings = parser.parse_warnings(sample_clang_tidy_output);

    EXPECT_EQ(warnings.size(), 4);

    // Check first warning (magic number)
    EXPECT_EQ(warnings[0].file_path, "/home/user/project/src/main.cpp");
    EXPECT_EQ(warnings[0].line_number, 42);
    EXPECT_EQ(warnings[0].warning_type, "readability-magic-numbers");
    EXPECT_FALSE(warnings[0].function_lines.has_value());

    // Check second warning (function size with note)
    EXPECT_EQ(warnings[1].file_path, "/home/user/project/src/parser.cpp");
    EXPECT_EQ(warnings[1].line_number, 78);
    EXPECT_EQ(warnings[1].warning_type, "readability-function-size");
    ASSERT_TRUE(warnings[1].function_lines.has_value());
    EXPECT_EQ(warnings[1].function_lines.value(), 44);

    // Check third warning (identifier naming)
    EXPECT_EQ(warnings[2].file_path, "/home/user/project/include/utils.hpp");
    EXPECT_EQ(warnings[2].line_number, 123);
    EXPECT_EQ(warnings[2].warning_type, "readability-identifier-naming");

    // Check fourth warning (performance)
    EXPECT_EQ(warnings[3].file_path, "/home/user/project/src/helper.cpp");
    EXPECT_EQ(warnings[3].warning_type, "performance-for-range-copy");
}

TEST_F(WarningParserTest, ClangTidyParser_HandleMalformedInput) {
    ClangTidyParser parser;

    auto warnings = parser.parse_warnings(malformed_clang_tidy_output);

    // Should skip malformed lines and only parse valid ones (none in this case)
    EXPECT_EQ(warnings.size(), 0);
}

TEST_F(WarningParserTest, ClangTidyParser_EmptyInput) {
    ClangTidyParser parser;

    auto warnings = parser.parse_warnings("");
    EXPECT_TRUE(warnings.empty());

    auto warnings2 = parser.parse_warnings("   \n\n  \n");
    EXPECT_TRUE(warnings2.empty());
}

TEST_F(WarningParserTest, ClangTidyParser_StreamInput) {
    ClangTidyParser parser;
    std::istringstream input_stream{sample_clang_tidy_output};

    auto warnings = parser.parse_warnings(input_stream);

    EXPECT_EQ(warnings.size(), 4);
    EXPECT_EQ(warnings[0].warning_type, "readability-magic-numbers");
    EXPECT_EQ(warnings[1].warning_type, "readability-function-size");
}

TEST_F(WarningParserTest, ClangTidyParser_OnlyNotesWithoutWarnings) {
    ClangTidyParser parser;
    std::string input = R"(
/src/file.cpp:10:1: note: 25 lines including whitespace
/src/file.cpp:20:1: note: 30 lines including comments
)";

    auto warnings = parser.parse_warnings(input);

    // Notes without preceding warnings should be ignored
    EXPECT_EQ(warnings.size(), 0);
}

TEST_F(WarningParserTest, ClangTidyParser_NonFunctionWarningWithNote) {
    ClangTidyParser parser;
    std::string input = R"(/src/main.cpp:42:5: warning: magic number [readability-magic-numbers]
/src/main.cpp:42:5: note: 10 lines including whitespace
)";

    auto warnings = parser.parse_warnings(input);

    ASSERT_EQ(warnings.size(), 1);
    // Note should be ignored because it's not a function-level warning
    EXPECT_FALSE(warnings[0].function_lines.has_value());
}

// Test edge cases and error conditions

TEST_F(WarningParserTest, ClangTidyParser_VeryLongLines) {
    ClangTidyParser parser;
    std::string very_long_message(1000, 'x');
    std::string input = "/src/file.cpp:1:1: warning: " + very_long_message + " [test-warning]\n";

    auto warnings = parser.parse_warnings(input);

    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].message, very_long_message);
}

TEST_F(WarningParserTest, ClangTidyParser_UnicodeInPaths) {
    ClangTidyParser parser;
    std::string input = "/home/user/проект/src/файл.cpp:1:1: warning: test [test-warning]\n";

    auto warnings = parser.parse_warnings(input);

    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].file_path, "/home/user/проект/src/файл.cpp");
}

TEST_F(WarningParserTest, ClangTidyParser_WindowsPaths) {
    ClangTidyParser parser;
    std::string input
        = "C:\\Users\\Name\\project\\src\\main.cpp:42:5: warning: test [test-warning]\n";

    auto warnings = parser.parse_warnings(input);

    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].file_path, "C:\\Users\\Name\\project\\src\\main.cpp");
}

// Integration test combining functional core and parser

TEST_F(WarningParserTest, Integration_ParseAndUseWithFunctionalCore) {
    ClangTidyParser parser;
    std::string input
        = "/src/main.cpp:42:5: warning: 42 is a magic number [readability-magic-numbers]\n";

    auto warnings = parser.parse_warnings(input);
    ASSERT_EQ(warnings.size(), 1);

    // Use with functional core
    std::vector<std::string> empty_file;
    auto modification = nolint::functional_core::create_modification(
        warnings[0], NolintStyle::NOLINT_SPECIFIC, empty_file);

    EXPECT_EQ(modification.warning_type, "readability-magic-numbers");
    EXPECT_EQ(modification.style, NolintStyle::NOLINT_SPECIFIC);
}