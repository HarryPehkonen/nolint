#include "nolint/core/annotated_file.hpp"
#include <gtest/gtest.h>

namespace nolint {

class AnnotatedFileTest : public ::testing::Test {
protected:
    std::vector<std::string> sample_lines_{
        "int main() {",
        "    int x = 42;  // Magic number warning here",
        "    return x;",
        "}"
    };
};

TEST_F(AnnotatedFileTest, CreateAnnotatedFile)
{
    auto file = create_annotated_file(sample_lines_);
    
    ASSERT_EQ(file.lines.size(), 4);
    EXPECT_EQ(file.lines[0].text, "int main() {");
    EXPECT_EQ(file.lines[1].text, "    int x = 42;  // Magic number warning here");
    EXPECT_TRUE(file.blocks.empty());
    
    // All annotations should be empty initially
    for (const auto& line : file.lines) {
        EXPECT_TRUE(line.before_comments.empty());
        EXPECT_FALSE(line.inline_comment.has_value());
    }
}

TEST_F(AnnotatedFileTest, RenderEmptyAnnotatedFile)
{
    auto file = create_annotated_file(sample_lines_);
    auto rendered = render_annotated_file(file);
    
    EXPECT_EQ(rendered, sample_lines_);
}

TEST_F(AnnotatedFileTest, RenderWithInlineComment)
{
    auto file = create_annotated_file(sample_lines_);
    file.lines[1].inline_comment = "// NOLINT(readability-magic-numbers)";
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 4);
    EXPECT_EQ(rendered[0], "int main() {");
    EXPECT_EQ(rendered[1], "    int x = 42;  // Magic number warning here  // NOLINT(readability-magic-numbers)");
    EXPECT_EQ(rendered[2], "    return x;");
    EXPECT_EQ(rendered[3], "}");
}

TEST_F(AnnotatedFileTest, RenderWithBeforeComment)
{
    auto file = create_annotated_file(sample_lines_);
    file.lines[1].before_comments.push_back("    // NOLINTNEXTLINE(readability-magic-numbers)");
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 5);
    EXPECT_EQ(rendered[0], "int main() {");
    EXPECT_EQ(rendered[1], "    // NOLINTNEXTLINE(readability-magic-numbers)");
    EXPECT_EQ(rendered[2], "    int x = 42;  // Magic number warning here");
    EXPECT_EQ(rendered[3], "    return x;");
    EXPECT_EQ(rendered[4], "}");
}

TEST_F(AnnotatedFileTest, RenderWithBlockSuppression)
{
    auto file = create_annotated_file(sample_lines_);
    file.blocks.push_back({
        .start_line = 0,
        .end_line = 3,
        .warning_type = "readability-function-size"
    });
    
    auto rendered = render_annotated_file(file);
    
    ASSERT_EQ(rendered.size(), 6);
    EXPECT_EQ(rendered[0], "// NOLINTBEGIN(readability-function-size)");
    EXPECT_EQ(rendered[1], "int main() {");
    EXPECT_EQ(rendered[2], "    int x = 42;  // Magic number warning here");
    EXPECT_EQ(rendered[3], "    return x;");
    EXPECT_EQ(rendered[4], "}");
    EXPECT_EQ(rendered[5], "// NOLINTEND(readability-function-size)");
}

TEST_F(AnnotatedFileTest, ExtractIndentation)
{
    EXPECT_EQ(extract_indentation("int main() {"), "");
    EXPECT_EQ(extract_indentation("    int x = 42;"), "    ");
    EXPECT_EQ(extract_indentation("\t\tint y = 0;"), "\t\t");
    EXPECT_EQ(extract_indentation("  \t  mixed;"), "  \t  ");
    EXPECT_EQ(extract_indentation(""), "");
    EXPECT_EQ(extract_indentation("   "), "");  // Only whitespace
}

// Critical edge case test: NOLINTBEGIN + NOLINTNEXTLINE on same line
TEST_F(AnnotatedFileTest, EdgeCaseMultipleSuppressionsSameLine)
{
    auto file = create_annotated_file(sample_lines_);
    
    // Apply NOLINT_BLOCK starting at line 1
    file.blocks.push_back({
        .start_line = 1,
        .end_line = 2,
        .warning_type = "readability-function-size"
    });
    
    // Apply NOLINTNEXTLINE at line 1 (same line as block start)
    file.lines[1].before_comments.push_back("    // NOLINTNEXTLINE(readability-magic-numbers)");
    
    auto rendered = render_annotated_file(file);
    
    // Expected order: NOLINTBEGIN first, then NOLINTNEXTLINE, then original line
    EXPECT_TRUE(std::find(rendered.begin(), rendered.end(), 
                         "    // NOLINTBEGIN(readability-function-size)") != rendered.end());
    EXPECT_TRUE(std::find(rendered.begin(), rendered.end(), 
                         "    // NOLINTNEXTLINE(readability-magic-numbers)") != rendered.end());
    
    // Find positions to verify order
    auto begin_it = std::find(rendered.begin(), rendered.end(), 
                             "    // NOLINTBEGIN(readability-function-size)");
    auto nextline_it = std::find(rendered.begin(), rendered.end(), 
                                "    // NOLINTNEXTLINE(readability-magic-numbers)");
    
    // NOLINTBEGIN must come before NOLINTNEXTLINE
    EXPECT_LT(std::distance(rendered.begin(), begin_it), 
              std::distance(rendered.begin(), nextline_it));
}

} // namespace nolint