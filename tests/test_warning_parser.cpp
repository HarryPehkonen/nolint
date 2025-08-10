#include "../include/warning_parser.hpp"
#include <gtest/gtest.h>
#include <sstream>

using namespace nolint;

TEST(WarningParserTest, ParseSingleWarning) {
    WarningParser parser;
    std::string input = "src/file.cpp:10:5: warning: unused variable 'x' [clang-diagnostic-unused-variable]";
    
    auto warnings = parser.parse(input);
    
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].file_path, "src/file.cpp");
    EXPECT_EQ(warnings[0].line_number, 10);
    EXPECT_EQ(warnings[0].column, 5);
    EXPECT_EQ(warnings[0].message, "unused variable 'x'");
    EXPECT_EQ(warnings[0].type, "clang-diagnostic-unused-variable");
}

TEST(WarningParserTest, ParseMultipleWarnings) {
    WarningParser parser;
    std::string input = 
        "file1.cpp:1:1: warning: message1 [type1]\n"
        "file2.cpp:2:2: warning: message2 [type2]\n"
        "file3.cpp:3:3: warning: message3 [type3]";
    
    auto warnings = parser.parse(input);
    
    ASSERT_EQ(warnings.size(), 3);
    EXPECT_EQ(warnings[0].file_path, "file1.cpp");
    EXPECT_EQ(warnings[1].file_path, "file2.cpp");
    EXPECT_EQ(warnings[2].file_path, "file3.cpp");
}

TEST(WarningParserTest, IgnoreNonWarningLines) {
    WarningParser parser;
    std::string input = 
        "Some random text\n"
        "file.cpp:10:5: warning: real warning [type]\n"
        "More random text\n"
        "   ^ note: some note";
    
    auto warnings = parser.parse(input);
    
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].message, "real warning");
}

TEST(WarningParserTest, HandleEmptyInput) {
    WarningParser parser;
    
    auto warnings = parser.parse("");
    
    EXPECT_EQ(warnings.size(), 0);
}

TEST(WarningParserTest, HandleComplexPaths) {
    WarningParser parser;
    std::string input = "/home/user/my-project/src/file.cpp:42:10: warning: complex path [type]";
    
    auto warnings = parser.parse(input);
    
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].file_path, "/home/user/my-project/src/file.cpp");
    EXPECT_EQ(warnings[0].line_number, 42);
}
