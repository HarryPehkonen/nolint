#include "tests/test_config.hpp"
#include "warning_parser.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <set>

using namespace nolint;

TEST(RealOutputTest, ParseActualClangTidyOutput) {
    ClangTidyParser parser;

    // Use configured test data path
    std::filesystem::path test_file
        = std::filesystem::path(TEST_DATA_DIR) / "sample_tidy_output.txt";
    std::ifstream file(test_file);

    if (!file.is_open()) {
        GTEST_SKIP() << "sample_tidy_output.txt not found at: " << test_file;
    }

    auto warnings = parser.parse_warnings(file);

    // Assert exact total warnings in the test file (from test output we saw it's 242)
    ASSERT_EQ(warnings.size(), 242);

    // Verify specific warnings we know are in the file

    // Find first warning: readability-identifier-length at line 13:15
    auto first_warning = std::find_if(warnings.begin(), warnings.end(), [](const Warning& w) {
        return w.line_number == 13 && w.column_number == 15
               && w.warning_type == "readability-identifier-length";
    });
    ASSERT_NE(first_warning, warnings.end());
    EXPECT_EQ(first_warning->file_path,
              "/home/user/project/benchmarks/benchmark_dom_access_compat.cpp");
    EXPECT_THAT(first_warning->message, ::testing::HasSubstr("variable name '_' is too short"));

    // Find magic number warning at line 20:29
    auto magic_warning = std::find_if(warnings.begin(), warnings.end(), [](const Warning& w) {
        return w.line_number == 20 && w.column_number == 29
               && w.warning_type == "readability-magic-numbers";
    });
    ASSERT_NE(magic_warning, warnings.end());
    EXPECT_EQ(magic_warning->file_path,
              "/home/user/project/benchmarks/benchmark_dom_access_compat.cpp");
    EXPECT_THAT(magic_warning->message, ::testing::HasSubstr("50 is a magic number"));

    // Count function-size warnings and verify they have line counts
    long function_warnings_count
        = std::count_if(warnings.begin(), warnings.end(), [](const Warning& w) {
              return w.warning_type == "readability-function-size";
          });

    long function_warnings_with_lines
        = std::count_if(warnings.begin(), warnings.end(), [](const Warning& w) {
              return w.warning_type == "readability-function-size" && w.function_lines.has_value();
          });

    // Assert exact counts based on test output we observed
    EXPECT_EQ(function_warnings_count, 23);
    EXPECT_EQ(function_warnings_with_lines, 23); // All function warnings should have line counts

    // Verify a specific function-size warning with known line count
    auto large_function = std::find_if(warnings.begin(), warnings.end(), [](const Warning& w) {
        return w.warning_type == "readability-function-size" && w.function_lines.has_value()
               && *w.function_lines == 86; // One of the largest functions we saw
    });
    ASSERT_NE(large_function, warnings.end());
    EXPECT_THAT(large_function->file_path, ::testing::HasSubstr("json_formatter.hpp"));

    // Count different warning types to ensure parsing diversity
    std::set<std::string> warning_types;
    for (const auto& w : warnings) {
        warning_types.insert(w.warning_type);
    }

    // Should have multiple warning types
    EXPECT_GE(warning_types.size(), 3);
    EXPECT_THAT(warning_types, ::testing::Contains("readability-identifier-length"));
    EXPECT_THAT(warning_types, ::testing::Contains("readability-magic-numbers"));
    EXPECT_THAT(warning_types, ::testing::Contains("readability-function-size"));
}