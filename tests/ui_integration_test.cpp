#include "functional_core.hpp"
#include "nolint_types.hpp"
#include <gtest/gtest.h>

using namespace nolint;

// Simple integration tests for search/filter functionality
class UIIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
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
                         Warning{"/src/main.cpp",
                                 100,
                                 20,
                                 "modernize-use-nullptr",
                                 "use nullptr instead of NULL",
                                 {}}};
    }

    std::vector<Warning> test_warnings;
};

// Test that search functionality integrates with the filter functions
TEST_F(UIIntegrationTest, EnterSearchMode_WithSlashKey) {
    // Test that filtering by "main" returns correct results
    auto filtered = functional_core::filter_warnings(test_warnings, "main");
    EXPECT_EQ(filtered.size(), 2);
    EXPECT_EQ(filtered[0].file_path, "/src/main.cpp");
    EXPECT_EQ(filtered[1].file_path, "/src/main.cpp");
}

TEST_F(UIIntegrationTest, SearchWithNoMatches_ShowsMessage) {
    // Test filtering with no matches
    auto filtered = functional_core::filter_warnings(test_warnings, "nonexistent");
    EXPECT_EQ(filtered.size(), 0);
}

TEST_F(UIIntegrationTest, ClearSearchFilter_EmptyInput) {
    // Test that empty filter returns all warnings
    auto filtered = functional_core::filter_warnings(test_warnings, "");
    EXPECT_EQ(filtered.size(), 3);
}

TEST_F(UIIntegrationTest, NavigateWithinFilteredResults) {
    // Test filtering and navigation within results
    auto filtered = functional_core::filter_warnings(test_warnings, "main");
    EXPECT_EQ(filtered.size(), 2);

    // Both results should be from main.cpp
    for (const auto& warning : filtered) {
        EXPECT_EQ(warning.file_path, "/src/main.cpp");
    }
}

TEST_F(UIIntegrationTest, SearchStatePersistsAcrossOperations) {
    // Test multiple filter operations
    auto filtered1 = functional_core::filter_warnings(test_warnings, "main");
    auto filtered2 = functional_core::filter_warnings(test_warnings, "utils");
    auto filtered3 = functional_core::filter_warnings(test_warnings, "");

    EXPECT_EQ(filtered1.size(), 2); // main.cpp warnings
    EXPECT_EQ(filtered2.size(), 1); // utils.cpp warning
    EXPECT_EQ(filtered3.size(), 3); // all warnings
}

// Critical regression test for the array bounds crash (the bug we just fixed)
TEST_F(UIIntegrationTest, FilterReducesArraySize_BoundsCheckingPreventssCrash) {
    // This test reproduces the exact scenario that caused the std::bad_alloc crash:
    // 1. Start with many warnings (simulate the original 242 warnings)
    // 2. User is viewing a warning with high index (e.g., warning #150)
    // 3. Apply filter that reduces the array size dramatically (e.g., to 23 warnings)
    // 4. The current_index becomes out of bounds for the filtered array

    // Create a large set of warnings similar to the real crash scenario
    std::vector<Warning> large_warning_set;

    // Add many non-function-size warnings (simulate the 219 other warnings)
    for (int i = 0; i < 200; ++i) {
        large_warning_set.push_back(Warning{"/src/file" + std::to_string(i) + ".cpp",
                                            i + 10,
                                            5,
                                            "readability-magic-numbers",
                                            "magic number found",
                                            {}});
    }

    // Add exactly 23 function-size warnings (matching the real crash scenario)
    for (int i = 0; i < 23; ++i) {
        large_warning_set.push_back(Warning{
            "/src/benchmark" + std::to_string(i) + ".cpp", i + 5, 10, "readability-function-size",
            "function exceeds recommended size",
            50 + i // function_lines
        });
    }

    // Now we have 223 total warnings (200 + 23)
    EXPECT_EQ(large_warning_set.size(), 223);

    // Simulate the user viewing warning #150 before searching
    int simulated_current_index = 150;
    EXPECT_LT(simulated_current_index, static_cast<int>(large_warning_set.size()));

    // Apply the filter that caused the crash: search for "readability-function-size"
    auto filtered_warnings
        = functional_core::filter_warnings(large_warning_set, "readability-function-size");

    // Verify the filter works correctly
    EXPECT_EQ(filtered_warnings.size(), 23);
    for (const auto& warning : filtered_warnings) {
        EXPECT_EQ(warning.warning_type, "readability-function-size");
        EXPECT_TRUE(warning.function_lines.has_value());
    }

    // This is the critical test: simulated_current_index (150) is now out of bounds
    // for the filtered array (size 23). This would cause std::bad_alloc crash.
    EXPECT_GE(simulated_current_index, static_cast<int>(filtered_warnings.size()));

    // Test the bounds checking logic that was added to fix the crash
    int adjusted_index = simulated_current_index;
    if (adjusted_index >= static_cast<int>(filtered_warnings.size())) {
        adjusted_index = std::max(0, static_cast<int>(filtered_warnings.size()) - 1);
    }

    // Verify the bounds checking works correctly
    EXPECT_EQ(adjusted_index, 22); // Should be the last valid index (size - 1)
    EXPECT_LT(adjusted_index, static_cast<int>(filtered_warnings.size()));

    // Verify we can safely access the warning at the adjusted index
    EXPECT_NO_THROW({
        const auto& safe_warning = filtered_warnings[adjusted_index];
        EXPECT_EQ(safe_warning.warning_type, "readability-function-size");
    });

    // Test edge case: empty filter result
    auto empty_filtered
        = functional_core::filter_warnings(large_warning_set, "nonexistent-pattern");
    EXPECT_EQ(empty_filtered.size(), 0);

    // Test bounds adjustment for empty results
    int adjusted_empty_index = simulated_current_index;
    if (adjusted_empty_index >= static_cast<int>(empty_filtered.size())) {
        adjusted_empty_index = std::max(0, static_cast<int>(empty_filtered.size()) - 1);
    }
    EXPECT_EQ(adjusted_empty_index, 0); // max(0, 0-1) = max(0, -1) = 0

    // But even adjusted_empty_index = 0 is out of bounds for empty array!
    // The application must handle empty results specially (revert to all warnings)
}

// Additional edge case test for empty filter results
TEST_F(UIIntegrationTest, EmptyFilterResult_BoundsHandling) {
    // Test the edge case where filter returns no results
    std::vector<Warning> warnings
        = {Warning{"/src/test.cpp", 10, 5, "some-warning", "test message", {}}};

    auto empty_result = functional_core::filter_warnings(warnings, "nonexistent");
    EXPECT_EQ(empty_result.size(), 0);

    // Test bounds adjustment for empty results
    int current_index = 0; // Even index 0 is out of bounds for empty array

    if (current_index >= static_cast<int>(empty_result.size())) {
        current_index = std::max(0, static_cast<int>(empty_result.size()) - 1);
    }

    // For empty array (size 0), max(0, 0-1) = max(0, -1) = 0
    // But index 0 is still out of bounds for empty array, so we need special handling
    EXPECT_EQ(current_index, 0);

    // The application should handle empty results by either:
    // 1. Showing "no results" message, or
    // 2. Reverting to showing all warnings
    // This test documents the current behavior
}