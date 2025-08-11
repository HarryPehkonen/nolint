#include "../include/annotated_file.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

using namespace nolint;

class AnnotatedFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test file
        std::ofstream file(test_file_);
        file << "int main() {\n";
        file << "    int unused_var = 42;\n";
        file << "    return 0;\n";
        file << "}\n";
    }
    
    void TearDown() override {
        std::filesystem::remove(test_file_);
    }
    
    const std::string test_file_ = "test_annotated.cpp";
};

TEST_F(AnnotatedFileTest, CreateFromLines) {
    std::vector<std::string> lines = {
        "line 1",
        "line 2", 
        "line 3"
    };
    
    auto file = create_annotated_file(lines);
    
    ASSERT_EQ(file.lines.size(), 3);
    EXPECT_EQ(file.lines[0].text, "line 1");
    EXPECT_EQ(file.lines[1].text, "line 2");
    EXPECT_EQ(file.lines[2].text, "line 3");
    EXPECT_TRUE(file.blocks.empty());
}

TEST_F(AnnotatedFileTest, LoadFromFile) {
    auto file = load_annotated_file(test_file_);
    
    ASSERT_EQ(file.lines.size(), 4);
    EXPECT_EQ(file.lines[0].text, "int main() {");
    EXPECT_EQ(file.lines[1].text, "    int unused_var = 42;");
    EXPECT_EQ(file.lines[2].text, "    return 0;");
    EXPECT_EQ(file.lines[3].text, "}");
}

TEST_F(AnnotatedFileTest, ApplyInlineNolint) {
    auto file = load_annotated_file(test_file_);
    Warning warning{test_file_, 2, 9, "clang-diagnostic-unused-variable", "unused variable", std::nullopt};
    
    auto modified = apply_decision(file, warning, NolintStyle::NOLINT);
    
    // Original file unchanged
    EXPECT_FALSE(file.lines[1].inline_comment.has_value());
    
    // Modified file has inline comment
    EXPECT_TRUE(modified.lines[1].inline_comment.has_value());
    EXPECT_EQ(*modified.lines[1].inline_comment, "// NOLINT(clang-diagnostic-unused-variable)");
}

TEST_F(AnnotatedFileTest, ApplyNolintnextline) {
    auto file = load_annotated_file(test_file_);
    Warning warning{test_file_, 2, 9, "clang-diagnostic-unused-variable", "unused variable", std::nullopt};
    
    auto modified = apply_decision(file, warning, NolintStyle::NOLINTNEXTLINE);
    
    // Modified file has before comment
    ASSERT_EQ(modified.lines[1].before_comments.size(), 1);
    EXPECT_EQ(modified.lines[1].before_comments[0], "    // NOLINTNEXTLINE(clang-diagnostic-unused-variable)");
}

TEST_F(AnnotatedFileTest, ApplyNolintBlock) {
    auto file = load_annotated_file(test_file_);
    Warning warning{test_file_, 2, 9, "clang-diagnostic-unused-variable", "unused variable", std::nullopt};
    
    auto modified = apply_decision(file, warning, NolintStyle::NOLINT_BLOCK);
    
    // Modified file has block suppression
    ASSERT_EQ(modified.blocks.size(), 1);
    EXPECT_EQ(modified.blocks[0].start_line, 1);  // 0-based index
    EXPECT_EQ(modified.blocks[0].end_line, 1);
    EXPECT_EQ(modified.blocks[0].warning_type, "clang-diagnostic-unused-variable");
}

TEST_F(AnnotatedFileTest, RenderInlineComment) {
    std::vector<std::string> lines = {"    int x = 42;"};
    auto file = create_annotated_file(lines);
    file.lines[0].inline_comment = "// NOLINT(type)";
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 1);
    EXPECT_EQ(rendered[0], "    int x = 42;  // NOLINT(type)");
}

TEST_F(AnnotatedFileTest, RenderNolintnextline) {
    std::vector<std::string> lines = {"    int x = 42;"};
    auto file = create_annotated_file(lines);
    file.lines[0].before_comments.push_back("    // NOLINTNEXTLINE(type)");
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 2);
    EXPECT_EQ(rendered[0], "    // NOLINTNEXTLINE(type)");
    EXPECT_EQ(rendered[1], "    int x = 42;");
}

TEST_F(AnnotatedFileTest, RenderNolintBlock) {
    std::vector<std::string> lines = {"    int x = 42;"};
    auto file = create_annotated_file(lines);
    file.blocks.push_back(BlockSuppression{0, 0, "type"});
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 3);
    EXPECT_EQ(rendered[0], "    // NOLINTBEGIN(type)");
    EXPECT_EQ(rendered[1], "    int x = 42;");
    EXPECT_EQ(rendered[2], "    // NOLINTEND(type)");
}

TEST_F(AnnotatedFileTest, RenderComplexCombination) {
    // Test the critical ordering: NOLINTBEGIN, NOLINTNEXTLINE, code line, NOLINTEND
    std::vector<std::string> lines = {"    int x = 42;"};
    auto file = create_annotated_file(lines);
    
    // Add NOLINT_BLOCK
    file.blocks.push_back(BlockSuppression{0, 0, "block-type"});
    
    // Add NOLINTNEXTLINE  
    file.lines[0].before_comments.push_back("    // NOLINTNEXTLINE(nextline-type)");
    
    // Add inline NOLINT
    file.lines[0].inline_comment = "// NOLINT(inline-type)";
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 4);
    EXPECT_EQ(rendered[0], "    // NOLINTBEGIN(block-type)");      // Block start first
    EXPECT_EQ(rendered[1], "    // NOLINTNEXTLINE(nextline-type)"); // NOLINTNEXTLINE second  
    EXPECT_EQ(rendered[2], "    int x = 42;  // NOLINT(inline-type)"); // Code with inline
    EXPECT_EQ(rendered[3], "    // NOLINTEND(block-type)");        // Block end last
}

TEST_F(AnnotatedFileTest, ExtractIndentation) {
    EXPECT_EQ(extract_indentation("no_indent"), "");
    EXPECT_EQ(extract_indentation("    four_spaces"), "    ");
    EXPECT_EQ(extract_indentation("\t\ttwo_tabs"), "\t\t");
    EXPECT_EQ(extract_indentation("  \t mixed"), "  \t ");  // Include space after tab
    EXPECT_EQ(extract_indentation("    "), "");  // Only whitespace
}

TEST_F(AnnotatedFileTest, HandleInvalidLineNumbers) {
    auto file = load_annotated_file(test_file_);
    Warning warning{test_file_, 100, 1, "type", "message", std::nullopt};  // Line 100 doesn't exist
    
    auto modified = apply_decision(file, warning, NolintStyle::NOLINT);
    
    // File should be unchanged for invalid line numbers
    EXPECT_EQ(file.lines.size(), modified.lines.size());
    EXPECT_EQ(file.blocks.size(), modified.blocks.size());
}

TEST_F(AnnotatedFileTest, ImmutableOperations) {
    auto original = load_annotated_file(test_file_);
    Warning warning{test_file_, 2, 9, "type", "message", std::nullopt};
    
    auto modified = apply_decision(original, warning, NolintStyle::NOLINT);
    
    // Original file should be completely unchanged
    EXPECT_FALSE(original.lines[1].inline_comment.has_value());
    EXPECT_TRUE(original.lines[1].before_comments.empty());
    EXPECT_TRUE(original.blocks.empty());
    
    // Modified file should have changes
    EXPECT_TRUE(modified.lines[1].inline_comment.has_value());
}
