#include "tests/test_config.hpp"
#include "warning_parser.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

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

    std::cout << "Parsed " << warnings.size() << " warnings\n";

    // Show first few warnings for debugging
    for (size_t i = 0; i < std::min(size_t(5), warnings.size()); ++i) {
        const auto& w = warnings[i];
        std::cout << "Warning " << i << ":\n";
        std::cout << "  File: " << w.file_path << "\n";
        std::cout << "  Line: " << w.line_number << ":" << w.column_number << "\n";
        std::cout << "  Type: " << w.warning_type << "\n";
        std::cout << "  Message: " << w.message << "\n";
        std::cout << "  Function lines: "
                  << (w.function_lines ? std::to_string(*w.function_lines) : "none") << "\n\n";
    }

    // Look for function-size warnings specifically
    int function_warnings = 0;
    int function_with_lines = 0;
    for (const auto& w : warnings) {
        if (w.warning_type == "readability-function-size") {
            function_warnings++;
            if (w.function_lines.has_value()) {
                function_with_lines++;
                std::cout << "Function-size warning with " << *w.function_lines << " lines in "
                          << w.file_path << "\n";
            }
        }
    }

    std::cout << "Found " << function_warnings << " function-size warnings\n";
    std::cout << "Found " << function_with_lines << " with line count notes\n";

    EXPECT_GT(warnings.size(), 0);
}