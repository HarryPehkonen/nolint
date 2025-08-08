#include "nolint/core/functional_core.hpp"
#include <gtest/gtest.h>

namespace nolint::functional_core {

class FunctionalCoreTest : public ::testing::Test {
protected:
    std::vector<Warning> sample_warnings_{
        Warning{
            .file_path = "/src/main.cpp",
            .line_number = 42,
            .column_number = 15,
            .warning_type = "readability-magic-numbers",
            .message = "42 is a magic number",
            .function_lines = std::nullopt
        },
        Warning{
            .file_path = "/src/parser.cpp",
            .line_number = 100,
            .column_number = 1,
            .warning_type = "readability-function-size",
            .message = "function is too large",
            .function_lines = 50
        },
        Warning{
            .file_path = "/src/main.cpp", 
            .line_number = 10,
            .column_number = 5,
            .warning_type = "modernize-use-auto",
            .message = "use auto instead of explicit type",
            .function_lines = std::nullopt
        }
    };
};

TEST_F(FunctionalCoreTest, FilterWarningsEmpty)
{
    auto indices = filter_warnings(sample_warnings_, "");
    
    EXPECT_EQ(indices.size(), 3);
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 1);
    EXPECT_EQ(indices[2], 2);
}

TEST_F(FunctionalCoreTest, FilterWarningsByType)
{
    auto indices = filter_warnings(sample_warnings_, "readability");
    
    EXPECT_EQ(indices.size(), 2);
    EXPECT_EQ(indices[0], 0);  // magic-numbers warning
    EXPECT_EQ(indices[1], 1);  // function-size warning
}

TEST_F(FunctionalCoreTest, FilterWarningsByFile)
{
    auto indices = filter_warnings(sample_warnings_, "main.cpp");
    
    EXPECT_EQ(indices.size(), 2);
    EXPECT_EQ(indices[0], 0);  // magic-numbers warning
    EXPECT_EQ(indices[1], 2);  // modernize-use-auto warning
}

TEST_F(FunctionalCoreTest, FilterWarningsMultiTermAnd)
{
    auto indices = filter_warnings(sample_warnings_, "main magic");
    
    EXPECT_EQ(indices.size(), 1);
    EXPECT_EQ(indices[0], 0);  // Only the magic-numbers warning in main.cpp
}

TEST_F(FunctionalCoreTest, FilterWarningsNoMatch)
{
    auto indices = filter_warnings(sample_warnings_, "nonexistent");
    
    EXPECT_TRUE(indices.empty());
}

TEST_F(FunctionalCoreTest, FilterWarningsCaseInsensitive)
{
    auto indices = filter_warnings(sample_warnings_, "READABILITY");
    
    EXPECT_EQ(indices.size(), 2);
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 1);
}

TEST_F(FunctionalCoreTest, SplitByWhitespace)
{
    auto tokens = split_by_whitespace("hello world test");
    
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(FunctionalCoreTest, SplitByWhitespaceEmpty)
{
    auto tokens = split_by_whitespace("");
    EXPECT_TRUE(tokens.empty());
}

TEST_F(FunctionalCoreTest, SplitByWhitespaceExtraSpaces)
{
    auto tokens = split_by_whitespace("  hello   world  ");
    
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
}

TEST_F(FunctionalCoreTest, ToLowercase)
{
    EXPECT_EQ(to_lowercase("HELLO"), "hello");
    EXPECT_EQ(to_lowercase("MiXeD"), "mixed");
    EXPECT_EQ(to_lowercase("already_lower"), "already_lower");
    EXPECT_EQ(to_lowercase(""), "");
}

TEST_F(FunctionalCoreTest, Trim)
{
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\nhello\r\n\t"), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "");
}

TEST_F(FunctionalCoreTest, CalculateStatistics)
{
    Decisions decisions;
    decisions[warning_key(sample_warnings_[0])] = NolintStyle::NOLINT_SPECIFIC;
    decisions[warning_key(sample_warnings_[2])] = NolintStyle::NONE;  // Not counted as addressed
    
    std::unordered_set<std::string> visited;
    visited.insert(warning_key(sample_warnings_[0]));
    visited.insert(warning_key(sample_warnings_[1]));
    
    auto stats = calculate_statistics(sample_warnings_, decisions, visited);
    
    EXPECT_EQ(stats.size(), 3);  // Three warning types
    
    // Find readability-magic-numbers stats
    auto magic_stats = std::find_if(stats.begin(), stats.end(),
                                   [](const auto& s) { return s.warning_type == "readability-magic-numbers"; });
    ASSERT_NE(magic_stats, stats.end());
    EXPECT_EQ(magic_stats->total_count, 1);
    EXPECT_EQ(magic_stats->addressed_count, 1);
    EXPECT_EQ(magic_stats->visited_count, 1);
    EXPECT_EQ(magic_stats->addressed_percentage(), 100);
}

TEST_F(FunctionalCoreTest, ApplyDecisionInlineComment)
{
    std::vector<std::string> lines = {"int x = 42;"};
    auto file = create_annotated_file(lines);
    
    Warning warning{
        .file_path = "/test.cpp",
        .line_number = 1,  // 1-based
        .column_number = 1,
        .warning_type = "readability-magic-numbers",
        .message = "test message",
        .function_lines = std::nullopt
    };
    
    apply_decision(file, warning, NolintStyle::NOLINT_SPECIFIC);
    
    EXPECT_TRUE(file.lines[0].inline_comment.has_value());
    EXPECT_EQ(*file.lines[0].inline_comment, "// NOLINT(readability-magic-numbers)");
}

TEST_F(FunctionalCoreTest, ApplyDecisionNolintnextline)
{
    std::vector<std::string> lines = {"    int x = 42;"};
    auto file = create_annotated_file(lines);
    
    Warning warning{
        .file_path = "/test.cpp",
        .line_number = 1,
        .column_number = 1,
        .warning_type = "readability-magic-numbers",
        .message = "test message",
        .function_lines = std::nullopt
    };
    
    apply_decision(file, warning, NolintStyle::NOLINTNEXTLINE);
    
    ASSERT_EQ(file.lines[0].before_comments.size(), 1);
    EXPECT_EQ(file.lines[0].before_comments[0], "    // NOLINTNEXTLINE(readability-magic-numbers)");
}

TEST_F(FunctionalCoreTest, HighlightNolintComments)
{
    std::string line = "int x = 42;  // NOLINT(readability-magic-numbers)";
    std::string highlighted = highlight_nolint_comments(line);
    
    // Should contain ANSI color codes
    EXPECT_NE(highlighted.find("\033[32m"), std::string::npos);  // Green start
    EXPECT_NE(highlighted.find("\033[0m"), std::string::npos);   // Reset
    
    // The entire comment including parentheses should be highlighted
    auto green_start = highlighted.find("\033[32m");
    auto green_end = highlighted.find("\033[0m", green_start);
    ASSERT_NE(green_start, std::string::npos);
    ASSERT_NE(green_end, std::string::npos);
    
    std::string highlighted_part = highlighted.substr(green_start + 5, green_end - green_start - 5); // Skip color codes
    EXPECT_NE(highlighted_part.find("// NOLINT(readability-magic-numbers)"), std::string::npos);
}

TEST_F(FunctionalCoreTest, HighlightDifferentNolintTypes)
{
    // Test NOLINTNEXTLINE
    std::string line1 = "// NOLINTNEXTLINE(readability-magic-numbers)";
    std::string highlighted1 = highlight_nolint_comments(line1);
    EXPECT_NE(highlighted1.find("\033[32m"), std::string::npos);
    EXPECT_NE(highlighted1.find("NOLINTNEXTLINE(readability-magic-numbers)"), std::string::npos);
    
    // Test NOLINTBEGIN
    std::string line2 = "// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)";
    std::string highlighted2 = highlight_nolint_comments(line2);
    EXPECT_NE(highlighted2.find("\033[32m"), std::string::npos);
    EXPECT_NE(highlighted2.find("NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)"), std::string::npos);
    
    // Test basic NOLINT
    std::string line3 = "int x = 42;  // NOLINT";
    std::string highlighted3 = highlight_nolint_comments(line3);
    EXPECT_NE(highlighted3.find("\033[32m"), std::string::npos);
}

TEST_F(FunctionalCoreTest, BuildDisplayContextBasic)
{
    // Test file with warning on line 3
    std::vector<std::string> file_lines = {
        "#include <iostream>",
        "",
        "int main() {",
        "    int x = 42;  // Magic number warning here",
        "    std::cout << x;",
        "    return 0;",
        "}"
    };
    
    Warning warning{
        .file_path = "test.cpp",
        .line_number = 4,  // 1-based line number 
        .column_number = 13,
        .warning_type = "readability-magic-numbers",
        .message = "42 is a magic number",
        .function_lines = std::nullopt
    };
    
    auto context = build_display_context(warning, file_lines, NolintStyle::NONE);
    
    // Should have context lines around the warning
    EXPECT_FALSE(context.context_lines.empty());
    
    // Should include the warning line and some surrounding context
    bool found_warning_line = false;
    for (const auto& line : context.context_lines) {
        if (line.find("int x = 42") != std::string::npos) {
            found_warning_line = true;
            // Warning line should be marked with ">>"
            EXPECT_NE(line.find(">>"), std::string::npos);
        }
    }
    EXPECT_TRUE(found_warning_line);
}

TEST_F(FunctionalCoreTest, BuildDisplayContextWithPreview)
{
    std::vector<std::string> file_lines = {
        "int main() {",
        "    int x = 42;",
        "    return 0;",
        "}"
    };
    
    Warning warning{
        .file_path = "test.cpp",
        .line_number = 2,
        .column_number = 13,
        .warning_type = "readability-magic-numbers",
        .message = "42 is a magic number",
        .function_lines = std::nullopt
    };
    
    // Test with NOLINT_SPECIFIC style to see preview
    auto context = build_display_context(warning, file_lines, NolintStyle::NOLINT_SPECIFIC);
    
    EXPECT_FALSE(context.context_lines.empty());
    EXPECT_EQ(context.format_preview, "// NOLINT(warning-type)");
}

TEST_F(FunctionalCoreTest, BuildDisplayContextWithNolintnextlineHighlighting)
{
    std::vector<std::string> file_lines = {
        "int main() {",
        "    int x = 42;",
        "    return 0;",
        "}"
    };
    
    Warning warning{
        .file_path = "test.cpp",
        .line_number = 2,
        .column_number = 13,
        .warning_type = "readability-magic-numbers",
        .message = "42 is a magic number",
        .function_lines = std::nullopt
    };
    
    // Test with NOLINTNEXTLINE style to see highlighted preview
    auto context = build_display_context(warning, file_lines, NolintStyle::NOLINTNEXTLINE);
    
    EXPECT_FALSE(context.context_lines.empty());
    
    // Find the preview line with NOLINTNEXTLINE
    bool found_highlighted_preview = false;
    for (const auto& line : context.context_lines) {
        if (line.find("NOLINTNEXTLINE") != std::string::npos) {
            // Should contain ANSI green color codes
            EXPECT_NE(line.find("\033[32m"), std::string::npos) << "Preview line should be green highlighted: " << line;
            EXPECT_NE(line.find("\033[0m"), std::string::npos) << "Preview line should have color reset: " << line;
            found_highlighted_preview = true;
            break;
        }
    }
    EXPECT_TRUE(found_highlighted_preview) << "Should find a NOLINTNEXTLINE preview line";
}

TEST_F(FunctionalCoreTest, ExistingNolintCommentsNotHighlighted)
{
    std::vector<std::string> file_lines = {
        "int main() {",
        "    int y = 123; // NOLINT(readability-magic-numbers) - existing comment",
        "    int x = 42;  // This line will get a warning", 
        "    return 0;",
        "}"
    };
    
    Warning warning{
        .file_path = "test.cpp",
        .line_number = 3, // Warning on line 3 (int x = 42)
        .column_number = 13,
        .warning_type = "readability-magic-numbers",
        .message = "42 is a magic number",
        .function_lines = std::nullopt
    };
    
    // Test with no suppression - should show context without highlighting existing NOLINT
    auto context = build_display_context(warning, file_lines, NolintStyle::NONE);
    
    EXPECT_FALSE(context.context_lines.empty());
    
    // Find the line with existing NOLINT comment
    bool found_existing_nolint_line = false;
    for (const auto& line : context.context_lines) {
        if (line.find("int y = 123") != std::string::npos) {
            // Should NOT contain ANSI green color codes
            EXPECT_EQ(line.find("\033[32m"), std::string::npos) << "Existing NOLINT should not be highlighted: " << line;
            EXPECT_EQ(line.find("\033[0m"), std::string::npos) << "Existing NOLINT should not have color codes: " << line;
            // But should contain the original NOLINT text
            EXPECT_NE(line.find("NOLINT(readability-magic-numbers)"), std::string::npos) << "Should contain existing NOLINT text: " << line;
            found_existing_nolint_line = true;
            break;
        }
    }
    EXPECT_TRUE(found_existing_nolint_line) << "Should find the existing NOLINT line";
}

TEST_F(FunctionalCoreTest, ProposedInlineNolintHighlighted)
{
    std::vector<std::string> file_lines = {
        "int main() {",
        "    int x = 42;  // This line will get a warning",
        "    return 0;",
        "}"
    };
    
    Warning warning{
        .file_path = "test.cpp",
        .line_number = 2,
        .column_number = 13,
        .warning_type = "readability-magic-numbers",
        .message = "42 is a magic number",
        .function_lines = std::nullopt
    };
    
    // Test with NOLINT_SPECIFIC style - should highlight the proposed inline NOLINT
    auto context = build_display_context(warning, file_lines, NolintStyle::NOLINT_SPECIFIC);
    
    EXPECT_FALSE(context.context_lines.empty());
    
    // Find the warning line with proposed inline NOLINT
    bool found_highlighted_warning_line = false;
    for (const auto& line : context.context_lines) {
        if (line.find("int x = 42") != std::string::npos && line.find("NOLINT(readability-magic-numbers)") != std::string::npos) {
            // Should contain ANSI green color codes for the proposed NOLINT
            EXPECT_NE(line.find("\033[32m"), std::string::npos) << "Proposed inline NOLINT should be highlighted: " << line;
            EXPECT_NE(line.find("\033[0m"), std::string::npos) << "Proposed inline NOLINT should have color reset: " << line;
            found_highlighted_warning_line = true;
            break;
        }
    }
    EXPECT_TRUE(found_highlighted_warning_line) << "Should find highlighted warning line with proposed NOLINT";
}

} // namespace nolint::functional_core