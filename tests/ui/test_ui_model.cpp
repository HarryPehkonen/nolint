#include "nolint/ui/ui_model.hpp"
#include <gtest/gtest.h>

namespace nolint {

class UIModelTest : public ::testing::Test {
protected:
    UIModel default_model_;
};

TEST_F(UIModelTest, InitialState)
{
    EXPECT_EQ(default_model_.current_index, 0);
    // REMOVED: current_style test - it's now always derived from decisions
    EXPECT_EQ(default_model_.get_current_style(), NolintStyle::NONE);
    EXPECT_EQ(default_model_.mode, ViewMode::REVIEWING);
    EXPECT_TRUE(default_model_.search_input.empty());
    EXPECT_TRUE(default_model_.filtered_indices.empty());
    EXPECT_EQ(default_model_.current_stats_index, 0);
    EXPECT_FALSE(default_model_.show_boundary_message);
    EXPECT_FALSE(default_model_.quit_confirmation_needed);
}

TEST_F(UIModelTest, GetActiveWarningCountNoFilter)
{
    // FIXED: Add warnings to the model
    default_model_.warnings.resize(100);
    EXPECT_EQ(default_model_.get_active_warning_count(), 100);
}

TEST_F(UIModelTest, GetActiveWarningCountWithFilter)
{
    // FIXED: Add warnings to the model and set up filtering
    default_model_.warnings.resize(100);
    default_model_.filtered_indices = {0, 5, 10, 15};
    EXPECT_EQ(default_model_.get_active_warning_count(), 4);
}

TEST_F(UIModelTest, GetActualWarningIndexNoFilter)
{
    default_model_.current_index = 42;
    EXPECT_EQ(default_model_.get_actual_warning_index(), 42);
}

TEST_F(UIModelTest, GetActualWarningIndexWithFilter)
{
    default_model_.filtered_indices = {10, 20, 30, 40};
    default_model_.current_index = 2;  // Third item in filtered list
    EXPECT_EQ(default_model_.get_actual_warning_index(), 30);
}

TEST_F(UIModelTest, WarningTypeStatsPercentage)
{
    WarningTypeStats stats{
        .warning_type = "readability-magic-numbers",
        .total_count = 10,
        .addressed_count = 3,
        .visited_count = 7
    };
    
    EXPECT_EQ(stats.addressed_percentage(), 30);
}

TEST_F(UIModelTest, WarningTypeStatsPercentageZeroTotal)
{
    WarningTypeStats stats{
        .warning_type = "test-warning",
        .total_count = 0,
        .addressed_count = 0,
        .visited_count = 0
    };
    
    EXPECT_EQ(stats.addressed_percentage(), 0);
}

TEST_F(UIModelTest, WarningTypeStatsPercentageFullyAddressed)
{
    WarningTypeStats stats{
        .warning_type = "test-warning",
        .total_count = 5,
        .addressed_count = 5,
        .visited_count = 5
    };
    
    EXPECT_EQ(stats.addressed_percentage(), 100);
}

TEST_F(UIModelTest, LineHighlighting)
{
    Line line{.text = "int x = 42;", .is_highlighted = true};
    EXPECT_TRUE(line.is_highlighted);
    EXPECT_EQ(line.text, "int x = 42;");
    
    Line normal_line{.text = "normal line"};
    EXPECT_FALSE(normal_line.is_highlighted);
}

TEST_F(UIModelTest, ScreenComposition)
{
    Screen screen;
    screen.content.push_back(Line{.text = "Line 1"});
    screen.content.push_back(Line{.text = "Line 2", .is_highlighted = true});
    screen.status_line = "Status";
    screen.control_hints = "Controls";
    
    ASSERT_EQ(screen.content.size(), 2);
    EXPECT_EQ(screen.content[0].text, "Line 1");
    EXPECT_FALSE(screen.content[0].is_highlighted);
    EXPECT_EQ(screen.content[1].text, "Line 2");
    EXPECT_TRUE(screen.content[1].is_highlighted);
    EXPECT_EQ(screen.status_line, "Status");
    EXPECT_EQ(screen.control_hints, "Controls");
}

TEST_F(UIModelTest, QuitConfirmationState)
{
    UIModel model;
    
    // Initially quit confirmation not needed
    EXPECT_FALSE(model.quit_confirmation_needed);
    
    // After setting quit confirmation needed
    model.quit_confirmation_needed = true;
    model.status_message = "Test confirmation message";
    
    EXPECT_TRUE(model.quit_confirmation_needed);
    EXPECT_EQ(model.status_message, "Test confirmation message");
}

TEST_F(UIModelTest, StatisticsNavigation)
{
    UIModel model;
    
    // Set up statistics data
    model.warning_stats = {
        WarningTypeStats{.warning_type = "first", .total_count = 1, .addressed_count = 0, .visited_count = 0},
        WarningTypeStats{.warning_type = "second", .total_count = 2, .addressed_count = 1, .visited_count = 1},
        WarningTypeStats{.warning_type = "third", .total_count = 3, .addressed_count = 2, .visited_count = 2}
    };
    
    // Initially at first item
    EXPECT_EQ(model.current_stats_index, 0);
    
    // Can navigate down
    model.current_stats_index++;
    EXPECT_EQ(model.current_stats_index, 1);
    
    model.current_stats_index++;
    EXPECT_EQ(model.current_stats_index, 2);
    
    // Can navigate back up
    model.current_stats_index--;
    EXPECT_EQ(model.current_stats_index, 1);
    
    model.current_stats_index--;
    EXPECT_EQ(model.current_stats_index, 0);
    
    // Bounds checking should be handled by application logic
}

} // namespace nolint