#include "../include/file_context.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

using namespace nolint;

class FileContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test file
        std::ofstream file(test_file_);
        file << "line 1\n";
        file << "line 2\n";
        file << "line 3\n";
        file << "line 4\n";
        file << "line 5\n";
        file << "line 6\n";
        file << "line 7\n";
    }
    
    void TearDown() override {
        // Clean up test file
        std::filesystem::remove(test_file_);
    }
    
    const std::string test_file_ = "test_context.cpp";
};

TEST_F(FileContextTest, ReadContextAroundWarning) {
    Warning warning{"test_context.cpp", 4, 1, "type", "message", std::nullopt};
    
    auto context = read_file_context(warning, 2);  // 2 lines of context
    
    EXPECT_TRUE(context.error_message.empty());
    ASSERT_EQ(context.lines.size(), 5);  // Lines 2-6 (2 before, 1 warning, 2 after)
    
    EXPECT_EQ(context.lines[0].line_number, 2);
    EXPECT_EQ(context.lines[0].text, "line 2");
    EXPECT_FALSE(context.lines[0].is_warning_line);
    
    EXPECT_EQ(context.lines[2].line_number, 4);
    EXPECT_EQ(context.lines[2].text, "line 4");
    EXPECT_TRUE(context.lines[2].is_warning_line);  // This is the warning line
}

TEST_F(FileContextTest, HandleFileNotFound) {
    Warning warning{"nonexistent.cpp", 1, 1, "type", "message", std::nullopt};
    
    auto context = read_file_context(warning, 3);
    
    EXPECT_FALSE(context.error_message.empty());
    EXPECT_EQ(context.lines.size(), 0);
}

TEST_F(FileContextTest, HandleLineOutOfRange) {
    Warning warning{"test_context.cpp", 100, 1, "type", "message", std::nullopt};
    
    auto context = read_file_context(warning, 3);
    
    EXPECT_FALSE(context.error_message.empty());
    EXPECT_EQ(context.lines.size(), 0);
}

TEST_F(FileContextTest, BuildSuppressionPreview) {
    Warning warning{"file.cpp", 10, 5, "readability-magic-numbers", "message", std::nullopt};
    
    auto nolint = build_suppression_preview(warning, NolintStyle::NOLINT);
    EXPECT_TRUE(nolint.has_value());
    EXPECT_EQ(*nolint, "  // NOLINT(readability-magic-numbers)");
    
    auto nextline = build_suppression_preview(warning, NolintStyle::NOLINTNEXTLINE);
    EXPECT_TRUE(nextline.has_value());
    EXPECT_EQ(*nextline, "// NOLINTNEXTLINE(readability-magic-numbers)");
    
    auto block = build_suppression_preview(warning, NolintStyle::NOLINT_BLOCK);
    EXPECT_TRUE(block.has_value());
    EXPECT_EQ(*block, "// NOLINTBEGIN(readability-magic-numbers)");
    
    auto none = build_suppression_preview(warning, NolintStyle::NONE);
    EXPECT_FALSE(none.has_value());
}
