#include "nolint/core/warning.hpp"
#include <gtest/gtest.h>

namespace nolint {

class WarningTest : public ::testing::Test {
protected:
    Warning test_warning_{
        .file_path = "/test.cpp",
        .line_number = 42,
        .column_number = 15,
        .warning_type = "readability-magic-numbers",
        .message = "42 is a magic number",
        .function_lines = std::nullopt
    };
};

TEST_F(WarningTest, WarningKeyGeneration)
{
    auto key = warning_key(test_warning_);
    EXPECT_EQ(key, "/test.cpp:42:15");
}

TEST_F(WarningTest, StyleCyclingUp)
{
    // Test normal warning (no function lines)
    auto style = NolintStyle::NONE;
    style = cycle_style_up(style, test_warning_);
    EXPECT_EQ(style, NolintStyle::NOLINT_SPECIFIC);
    
    style = cycle_style_up(style, test_warning_);
    EXPECT_EQ(style, NolintStyle::NOLINTNEXTLINE);
    
    style = cycle_style_up(style, test_warning_);
    EXPECT_EQ(style, NolintStyle::NONE);  // Skip NOLINT_BLOCK for non-function warnings
}

TEST_F(WarningTest, StyleCyclingUpWithFunctionLines)
{
    Warning function_warning = test_warning_;
    function_warning.function_lines = 50;
    
    auto style = NolintStyle::NONE;
    style = cycle_style_up(style, function_warning);
    EXPECT_EQ(style, NolintStyle::NOLINT_SPECIFIC);
    
    style = cycle_style_up(style, function_warning);
    EXPECT_EQ(style, NolintStyle::NOLINTNEXTLINE);
    
    style = cycle_style_up(style, function_warning);
    EXPECT_EQ(style, NolintStyle::NOLINT_BLOCK);  // Available for function warnings
    
    style = cycle_style_up(style, function_warning);
    EXPECT_EQ(style, NolintStyle::NONE);
}

TEST_F(WarningTest, StyleAvailability)
{
    EXPECT_TRUE(is_style_available(NolintStyle::NONE, test_warning_));
    EXPECT_TRUE(is_style_available(NolintStyle::NOLINT_SPECIFIC, test_warning_));
    EXPECT_TRUE(is_style_available(NolintStyle::NOLINTNEXTLINE, test_warning_));
    EXPECT_FALSE(is_style_available(NolintStyle::NOLINT_BLOCK, test_warning_));
    
    Warning function_warning = test_warning_;
    function_warning.function_lines = 50;
    EXPECT_TRUE(is_style_available(NolintStyle::NOLINT_BLOCK, function_warning));
}

TEST_F(WarningTest, StyleDisplayNames)
{
    EXPECT_EQ(style_display_name(NolintStyle::NONE), "No suppression");
    EXPECT_EQ(style_display_name(NolintStyle::NOLINT_SPECIFIC), "// NOLINT(warning-type)");
    EXPECT_EQ(style_display_name(NolintStyle::NOLINTNEXTLINE), "// NOLINTNEXTLINE(warning-type)");
    EXPECT_EQ(style_display_name(NolintStyle::NOLINT_BLOCK), 
              "// NOLINTBEGIN(warning-type) ... // NOLINTEND(warning-type)");
}

TEST_F(WarningTest, BidirectionalCycling)
{
    // Test that up and down cycling are reverse operations
    NolintStyle original = NolintStyle::NOLINT_SPECIFIC;
    
    // Cycle up then down should return to original
    auto up_once = cycle_style_up(original, test_warning_);
    auto back_down = cycle_style_down(up_once, test_warning_);
    EXPECT_EQ(back_down, original);
    
    // Cycle down then up should return to original  
    auto down_once = cycle_style_down(original, test_warning_);
    auto back_up = cycle_style_up(down_once, test_warning_);
    EXPECT_EQ(back_up, original);
    
    // Test complete cycles for consistency
    NolintStyle current = NolintStyle::NONE;
    
    // Up cycle: NONE -> NOLINT_SPECIFIC -> NOLINTNEXTLINE -> NONE
    current = cycle_style_up(current, test_warning_);
    EXPECT_EQ(current, NolintStyle::NOLINT_SPECIFIC);
    
    current = cycle_style_up(current, test_warning_);
    EXPECT_EQ(current, NolintStyle::NOLINTNEXTLINE);
    
    current = cycle_style_up(current, test_warning_);
    EXPECT_EQ(current, NolintStyle::NONE);
    
    // Down cycle: NONE -> NOLINTNEXTLINE -> NOLINT_SPECIFIC -> NONE
    current = NolintStyle::NONE;
    current = cycle_style_down(current, test_warning_);
    EXPECT_EQ(current, NolintStyle::NOLINTNEXTLINE);
    
    current = cycle_style_down(current, test_warning_);
    EXPECT_EQ(current, NolintStyle::NOLINT_SPECIFIC);
    
    current = cycle_style_down(current, test_warning_);
    EXPECT_EQ(current, NolintStyle::NONE);
}

} // namespace nolint