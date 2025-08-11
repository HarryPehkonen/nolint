#include "../include/ui_model.hpp"
#include <gtest/gtest.h>

using namespace nolint;

class UIModelTest : public ::testing::Test {
protected:
    UIModel create_test_model() {
        UIModel model;
        model.warnings = {
            {"file1.cpp", 10, 5, "type1", "message1", std::nullopt},
            {"file2.cpp", 20, 10, "type2", "message2", std::nullopt},
            {"file3.cpp", 30, 15, "type3", "message3", std::nullopt}
        };
        // Initialize filtered indices to show all warnings (no filter)
        model.filtered_warning_indices = filter_warnings(model.warnings, "");
        return model;
    }
};

// Pure function tests - no mocking needed!

TEST_F(UIModelTest, NavigateRight) {
    auto model = create_test_model();
    model.current_index = 0;
    
    auto new_model = update(model, InputEvent::ARROW_RIGHT);
    
    EXPECT_EQ(new_model.current_index, 1);
    EXPECT_EQ(model.current_index, 0);  // Original unchanged (immutable!)
}

TEST_F(UIModelTest, NavigateRightAtEnd) {
    auto model = create_test_model();
    model.current_index = 2;  // Last warning
    
    auto new_model = update(model, InputEvent::ARROW_RIGHT);
    
    EXPECT_EQ(new_model.current_index, 2);  // Stays at end
}

TEST_F(UIModelTest, NavigateLeft) {
    auto model = create_test_model();
    model.current_index = 2;
    
    auto new_model = update(model, InputEvent::ARROW_LEFT);
    
    EXPECT_EQ(new_model.current_index, 1);
}

TEST_F(UIModelTest, NavigateLeftAtStart) {
    auto model = create_test_model();
    model.current_index = 0;
    
    auto new_model = update(model, InputEvent::ARROW_LEFT);
    
    EXPECT_EQ(new_model.current_index, 0);  // Stays at start
}

TEST_F(UIModelTest, CycleStyleUp) {
    auto model = create_test_model();
    model.current_index = 0;
    
    // Add function_lines to first warning to enable NOLINT_BLOCK
    model.warnings[0].function_lines = 50;
    
    // Start at NONE
    EXPECT_EQ(model.current_style(), NolintStyle::NONE);
    
    // Cycle to NOLINT
    auto model1 = update(model, InputEvent::ARROW_UP);
    EXPECT_EQ(model1.current_style(), NolintStyle::NOLINT);
    
    // Cycle to NOLINTNEXTLINE
    auto model2 = update(model1, InputEvent::ARROW_UP);
    EXPECT_EQ(model2.current_style(), NolintStyle::NOLINTNEXTLINE);
    
    // Cycle to NOLINT_BLOCK (available because function_lines is set)
    auto model3 = update(model2, InputEvent::ARROW_UP);
    EXPECT_EQ(model3.current_style(), NolintStyle::NOLINT_BLOCK);
    
    // Cycle back to NONE
    auto model4 = update(model3, InputEvent::ARROW_UP);
    EXPECT_EQ(model4.current_style(), NolintStyle::NONE);
}

TEST_F(UIModelTest, CycleStyleDown) {
    auto model = create_test_model();
    model.current_index = 0;
    
    // Add function_lines to first warning to enable NOLINT_BLOCK
    model.warnings[0].function_lines = 50;
    
    // Start at NONE, cycle backwards to NOLINT_BLOCK
    auto model1 = update(model, InputEvent::ARROW_DOWN);
    EXPECT_EQ(model1.current_style(), NolintStyle::NOLINT_BLOCK);
}

TEST_F(UIModelTest, CycleStyleSkipsNolintBlockWhenNoFunctionLines) {
    auto model = create_test_model();
    model.current_index = 0;
    
    // No function_lines set - should skip NOLINT_BLOCK
    
    // Start at NONE
    EXPECT_EQ(model.current_style(), NolintStyle::NONE);
    
    // Cycle to NOLINT
    auto model1 = update(model, InputEvent::ARROW_UP);
    EXPECT_EQ(model1.current_style(), NolintStyle::NOLINT);
    
    // Cycle to NOLINTNEXTLINE
    auto model2 = update(model1, InputEvent::ARROW_UP);
    EXPECT_EQ(model2.current_style(), NolintStyle::NOLINTNEXTLINE);
    
    // Cycle should skip NOLINT_BLOCK and go back to NONE
    auto model3 = update(model2, InputEvent::ARROW_UP);
    EXPECT_EQ(model3.current_style(), NolintStyle::NONE);
}

TEST_F(UIModelTest, DecisionsPersistAcrossNavigation) {
    auto model = create_test_model();
    
    // Set style for first warning
    auto model1 = update(model, InputEvent::ARROW_UP);  // NONE -> NOLINT
    EXPECT_EQ(model1.current_style(), NolintStyle::NOLINT);
    
    // Navigate to next warning
    auto model2 = update(model1, InputEvent::ARROW_RIGHT);
    EXPECT_EQ(model2.current_index, 1);
    EXPECT_EQ(model2.current_style(), NolintStyle::NONE);  // Second warning has no decision yet
    
    // Navigate back
    auto model3 = update(model2, InputEvent::ARROW_LEFT);
    EXPECT_EQ(model3.current_index, 0);
    EXPECT_EQ(model3.current_style(), NolintStyle::NOLINT);  // Decision preserved!
}

TEST_F(UIModelTest, QuitSetsExitFlag) {
    auto model = create_test_model();
    EXPECT_FALSE(model.should_exit);
    
    auto new_model = update(model, InputEvent::QUIT);
    
    EXPECT_TRUE(new_model.should_exit);
    EXPECT_FALSE(model.should_exit);  // Original unchanged
}

TEST_F(UIModelTest, ToggleStatistics) {
    auto model = create_test_model();
    EXPECT_FALSE(model.show_statistics);
    
    auto model1 = update(model, InputEvent::SHOW_STATISTICS);
    EXPECT_TRUE(model1.show_statistics);
    
    auto model2 = update(model1, InputEvent::SHOW_STATISTICS);
    EXPECT_FALSE(model2.show_statistics);
}

TEST_F(UIModelTest, EmptyWarningsHandling) {
    UIModel model;  // No warnings
    model.filtered_warning_indices = filter_warnings(model.warnings, "");  // Empty vector
    
    // Should handle all events gracefully
    auto model1 = update(model, InputEvent::ARROW_RIGHT);
    EXPECT_EQ(model1.current_index, 0);
    
    auto model2 = update(model, InputEvent::ARROW_UP);
    EXPECT_EQ(model2.decisions.size(), 0);
    
    auto model3 = update(model, InputEvent::QUIT);
    EXPECT_TRUE(model3.should_exit);
}
