#include "nolint/parsers/warning_parser.hpp"
#include <gtest/gtest.h>

namespace nolint {

class WarningParserTest : public ::testing::Test {
protected:
    WarningParser parser_;
    
    std::string sample_output_ = R"(
/src/main.cpp:42:15: warning: 42 is a magic number, consider using a named constant instead [readability-magic-numbers]
/src/parser.cpp:100:1: warning: function 'parse_data' exceeds recommended size/complexity thresholds [readability-function-size]
/src/parser.cpp:100:1: note: 75 lines including whitespace and comments (threshold 50)
/src/utils.cpp:25:8: warning: use 'auto' when initializing with a template cast to avoid duplicating the type name [modernize-use-auto]
)";
};

TEST_F(WarningParserTest, ParseBasicWarning)
{
    std::string single_warning = 
        "/src/main.cpp:42:15: warning: 42 is a magic number [readability-magic-numbers]";
    
    auto warnings = parser_.parse_warnings(single_warning);
    
    ASSERT_EQ(warnings.size(), 1);
    
    const auto& warning = warnings[0];
    EXPECT_EQ(warning.file_path, "/src/main.cpp");
    EXPECT_EQ(warning.line_number, 42);
    EXPECT_EQ(warning.column_number, 15);
    EXPECT_EQ(warning.warning_type, "readability-magic-numbers");
    EXPECT_EQ(warning.message, "42 is a magic number");
    EXPECT_FALSE(warning.function_lines.has_value());
}

TEST_F(WarningParserTest, ParseMultipleWarnings)
{
    auto warnings = parser_.parse_warnings(sample_output_);
    
    ASSERT_EQ(warnings.size(), 3);
    
    // First warning
    EXPECT_EQ(warnings[0].file_path, "/src/main.cpp");
    EXPECT_EQ(warnings[0].warning_type, "readability-magic-numbers");
    EXPECT_FALSE(warnings[0].function_lines.has_value());
    
    // Second warning with function lines note
    EXPECT_EQ(warnings[1].file_path, "/src/parser.cpp");
    EXPECT_EQ(warnings[1].warning_type, "readability-function-size");
    EXPECT_TRUE(warnings[1].function_lines.has_value());
    EXPECT_EQ(*warnings[1].function_lines, 75);
    
    // Third warning
    EXPECT_EQ(warnings[2].file_path, "/src/utils.cpp");
    EXPECT_EQ(warnings[2].warning_type, "modernize-use-auto");
    EXPECT_FALSE(warnings[2].function_lines.has_value());
}

TEST_F(WarningParserTest, ParseWarningWithFunctionLinesNote)
{
    std::string warning_with_note = R"(
/src/big_function.cpp:10:1: warning: function exceeds size limits [readability-function-size]
/src/big_function.cpp:10:1: note: 125 lines including whitespace and comments (threshold 50)
)";
    
    auto warnings = parser_.parse_warnings(warning_with_note);
    
    ASSERT_EQ(warnings.size(), 1);
    
    const auto& warning = warnings[0];
    EXPECT_EQ(warning.warning_type, "readability-function-size");
    EXPECT_TRUE(warning.function_lines.has_value());
    EXPECT_EQ(*warning.function_lines, 125);
}

TEST_F(WarningParserTest, ParseEmptyInput)
{
    auto warnings = parser_.parse_warnings("");
    EXPECT_TRUE(warnings.empty());
}

TEST_F(WarningParserTest, ParseMalformedInput)
{
    std::string malformed = R"(
This is not a clang-tidy warning
/src/file.cpp: some random text
invalid:line:format
)";
    
    auto warnings = parser_.parse_warnings(malformed);
    EXPECT_TRUE(warnings.empty());
}

TEST_F(WarningParserTest, ParseMixedValidInvalid)
{
    std::string mixed = R"(
This line is invalid
/src/main.cpp:42:15: warning: valid warning [readability-magic-numbers]
Another invalid line
/src/test.cpp:10:5: warning: another valid warning [modernize-use-auto]
)";
    
    auto warnings = parser_.parse_warnings(mixed);
    
    ASSERT_EQ(warnings.size(), 2);
    EXPECT_EQ(warnings[0].warning_type, "readability-magic-numbers");
    EXPECT_EQ(warnings[1].warning_type, "modernize-use-auto");
}

} // namespace nolint