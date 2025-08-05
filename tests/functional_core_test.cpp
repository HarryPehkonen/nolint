#include "functional_core.hpp"
#include <algorithm>
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace nolint;
using namespace nolint::functional_core;

class FunctionalCoreTest : public ::testing::Test {
protected:
    // Test data using C++20 designated initializers
    const Warning magic_number_warning{.file_path = "/test.cpp",
                                       .line_number = 5,
                                       .column_number = 20,
                                       .warning_type = "readability-magic-numbers",
                                       .message = "42 is a magic number",
                                       .function_lines = std::nullopt};

    const Warning function_size_warning{.file_path = "/test.cpp",
                                        .line_number = 10,
                                        .column_number = 1,
                                        .warning_type = "readability-function-size",
                                        .message = "function is too long",
                                        .function_lines
                                        = 8}; // Function from line 7 to 14 = 8 lines

    const std::vector<std::string> sample_code{
        "class TestClass {",                       // 1
        "private:",                                // 2
        "    int value;",                          // 3
        "public:",                                 // 4
        "    int get_magic() { return 42; }",      // 5 - magic number warning
        "",                                        // 6
        "    void complex_function() {",           // 7
        "        // Start of function",            // 8
        "        int x = 1;",                      // 9
        "        for (int i = 0; i < 100; ++i) {", // 10 - function size warning
        "            x += i;",                     // 11
        "        }",                               // 12
        "        return x;",                       // 13
        "    }",                                   // 14
        "};"                                       // 15
    };
};

// Test pure text transformation functions

TEST_F(FunctionalCoreTest, ExtractIndentation_EmptyLine) { EXPECT_EQ(extract_indentation(""), ""); }

TEST_F(FunctionalCoreTest, ExtractIndentation_NoIndent) {
    EXPECT_EQ(extract_indentation("class Foo {"), "");
}

TEST_F(FunctionalCoreTest, ExtractIndentation_SpaceIndent) {
    EXPECT_EQ(extract_indentation("    int value;"), "    ");
}

TEST_F(FunctionalCoreTest, ExtractIndentation_TabIndent) {
    EXPECT_EQ(extract_indentation("\t\tint value;"), "\t\t");
}

TEST_F(FunctionalCoreTest, ExtractIndentation_MixedIndent) {
    EXPECT_EQ(extract_indentation("  \t  int value;"), "  \t  ");
}

TEST_F(FunctionalCoreTest, ExtractIndentation_AllWhitespace) {
    const std::string line = "    \t  ";
    EXPECT_EQ(extract_indentation(line), line);
}

TEST_F(FunctionalCoreTest, FormatNolintComment_Specific) {
    auto result = format_nolint_comment(NolintStyle::NOLINT_SPECIFIC, "readability-magic-numbers");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "// NOLINT(readability-magic-numbers)");
}

TEST_F(FunctionalCoreTest, FormatNolintComment_NextLine) {
    auto result
        = format_nolint_comment(NolintStyle::NOLINTNEXTLINE, "readability-magic-numbers", "    ");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "    // NOLINTNEXTLINE(readability-magic-numbers)");
}

TEST_F(FunctionalCoreTest, FormatNolintComment_Block) {
    auto result
        = format_nolint_comment(NolintStyle::NOLINT_BLOCK, "readability-function-size", "    ");
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "    // NOLINTBEGIN(readability-function-size)");
    EXPECT_EQ(result[1], "    // NOLINTEND(readability-function-size)");
}

TEST_F(FunctionalCoreTest, FormatNolintComment_None) {
    auto result = format_nolint_comment(NolintStyle::NONE, "any-warning");
    EXPECT_TRUE(result.empty());
}

TEST_F(FunctionalCoreTest, CycleNolintStyle_WithoutBlock) {
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NONE, false), NolintStyle::NOLINT_SPECIFIC);
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NOLINT_SPECIFIC, false), NolintStyle::NOLINTNEXTLINE);
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NOLINTNEXTLINE, false), NolintStyle::NONE);
}

TEST_F(FunctionalCoreTest, CycleNolintStyle_WithBlock) {
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NONE, true), NolintStyle::NOLINT_SPECIFIC);
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NOLINT_SPECIFIC, true), NolintStyle::NOLINTNEXTLINE);
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NOLINTNEXTLINE, true), NolintStyle::NOLINT_BLOCK);
    EXPECT_EQ(cycle_nolint_style(NolintStyle::NOLINT_BLOCK, true), NolintStyle::NONE);
}

TEST_F(FunctionalCoreTest, IsFunctionSignature_Basic) {
    EXPECT_TRUE(is_function_signature("void foo() {"));
    EXPECT_TRUE(is_function_signature("    int calculate(int x, int y) {"));
    EXPECT_TRUE(is_function_signature("auto get_value() -> int {"));
}

TEST_F(FunctionalCoreTest, IsFunctionSignature_NotFunction) {
    EXPECT_FALSE(is_function_signature("// This is a comment"));
    EXPECT_FALSE(is_function_signature("/* Block comment */"));
    EXPECT_FALSE(is_function_signature("if (condition) {"));
    EXPECT_FALSE(is_function_signature("while (true) {"));
    EXPECT_FALSE(is_function_signature("for (int i = 0; i < 10; ++i) {"));
    EXPECT_FALSE(is_function_signature("class ClassName {"));
}

TEST_F(FunctionalCoreTest, CreateModification_None) {
    auto mod = create_modification(magic_number_warning, NolintStyle::NONE, sample_code);

    EXPECT_EQ(mod.target_line, 5);
    EXPECT_EQ(mod.style, NolintStyle::NONE);
    EXPECT_EQ(mod.warning_type, "readability-magic-numbers");
}

TEST_F(FunctionalCoreTest, CreateModification_Specific) {
    auto mod = create_modification(magic_number_warning, NolintStyle::NOLINT_SPECIFIC, sample_code);

    EXPECT_EQ(mod.target_line, 5);
    EXPECT_EQ(mod.style, NolintStyle::NOLINT_SPECIFIC);
    EXPECT_EQ(mod.warning_type, "readability-magic-numbers");
    EXPECT_EQ(mod.indentation, "    "); // Line 5 has 4-space indent
}

TEST_F(FunctionalCoreTest, CreateModification_Block) {
    auto mod = create_modification(function_size_warning, NolintStyle::NOLINT_BLOCK, sample_code);

    EXPECT_EQ(mod.target_line, 10);
    EXPECT_EQ(mod.style, NolintStyle::NOLINT_BLOCK);
    EXPECT_EQ(mod.warning_type, "readability-function-size");
    EXPECT_TRUE(mod.block_start_line.has_value());
    EXPECT_TRUE(mod.block_end_line.has_value());

    // Should find function boundaries
    EXPECT_EQ(mod.block_start_line.value(), 7); // "void complex_function() {"
    EXPECT_EQ(mod.block_end_line.value(), 15);  // After "}" (NOLINTEND goes after closing brace)
}

// Test text transformations - the core business logic

TEST_F(FunctionalCoreTest, ApplyModification_None_NoChange) {
    Modification mod{.target_line = 5,
                     .style = NolintStyle::NONE,
                     .warning_type = "readability-magic-numbers",
                     .indentation = "",
                     .block_start_line = std::nullopt,
                     .block_end_line = std::nullopt};

    auto result = apply_modification_to_lines(sample_code, mod);

    EXPECT_EQ(result.lines_added, 0);
    EXPECT_EQ(result.lines_removed, 0);
    EXPECT_EQ(result.lines, sample_code); // Unchanged
}

TEST_F(FunctionalCoreTest, ApplyModification_Specific_InlineComment) {
    Modification mod{.target_line = 5,
                     .style = NolintStyle::NOLINT_SPECIFIC,
                     .warning_type = "readability-magic-numbers",
                     .indentation = "    ",
                     .block_start_line = std::nullopt,
                     .block_end_line = std::nullopt};

    auto result = apply_modification_to_lines(sample_code, mod);

    EXPECT_EQ(result.lines_added, 0);
    EXPECT_EQ(result.lines_removed, 0);
    EXPECT_EQ(result.lines.size(), sample_code.size());
    EXPECT_EQ(result.lines[4],
              "    int get_magic() { return 42; }  // NOLINT(readability-magic-numbers)");
}

TEST_F(FunctionalCoreTest, ApplyModification_NextLine_InsertBefore) {
    Modification mod{.target_line = 5,
                     .style = NolintStyle::NOLINTNEXTLINE,
                     .warning_type = "readability-magic-numbers",
                     .indentation = "    ",
                     .block_start_line = std::nullopt,
                     .block_end_line = std::nullopt};

    auto result = apply_modification_to_lines(sample_code, mod);

    EXPECT_EQ(result.lines_added, 1);
    EXPECT_EQ(result.lines_removed, 0);
    EXPECT_EQ(result.lines.size(), sample_code.size() + 1);
    EXPECT_EQ(result.lines[4], "    // NOLINTNEXTLINE(readability-magic-numbers)");
    EXPECT_EQ(result.lines[5], "    int get_magic() { return 42; }");
}

TEST_F(FunctionalCoreTest, ApplyModification_Block_BeginEnd) {
    Modification mod{.target_line = 10,
                     .style = NolintStyle::NOLINT_BLOCK,
                     .warning_type = "readability-function-size",
                     .indentation = "    ",
                     .block_start_line = 7,
                     .block_end_line = 14};

    auto result = apply_modification_to_lines(sample_code, mod);

    EXPECT_EQ(result.lines_added, 2);
    EXPECT_EQ(result.lines_removed, 0);
    EXPECT_EQ(result.lines.size(), sample_code.size() + 2);

    // NOLINTBEGIN inserted before line 7 (index 6)
    EXPECT_EQ(result.lines[6], "    // NOLINTBEGIN(readability-function-size)");
    EXPECT_EQ(result.lines[7], "    void complex_function() {");

    // NOLINTEND inserted after line 14 (adjusted for insertion, now at index 15)
    EXPECT_EQ(result.lines[15], "    // NOLINTEND(readability-function-size)");
}

TEST_F(FunctionalCoreTest, ApplyMultipleModifications_CorrectLineNumbers) {
    // Apply modifications in order that tests line number handling
    std::vector<Modification> modifications{{.target_line = 5,
                                             .style = NolintStyle::NOLINT_SPECIFIC,
                                             .warning_type = "readability-magic-numbers",
                                             .indentation = "    ",
                                             .block_start_line = std::nullopt,
                                             .block_end_line = std::nullopt},
                                            {.target_line = 10,
                                             .style = NolintStyle::NOLINTNEXTLINE,
                                             .warning_type = "readability-function-size",
                                             .indentation = "        ",
                                             .block_start_line = std::nullopt,
                                             .block_end_line = std::nullopt}};

    auto result = apply_modifications_to_lines(sample_code, modifications);

    EXPECT_EQ(result.lines_added, 1); // Only NOLINTNEXTLINE adds a line
    EXPECT_EQ(result.lines_removed, 0);

    // Check that both modifications were applied correctly
    EXPECT_THAT(result.lines[4], ::testing::HasSubstr("// NOLINT(readability-magic-numbers)"));
    EXPECT_EQ(result.lines[9], "        // NOLINTNEXTLINE(readability-function-size)");
}

TEST_F(FunctionalCoreTest, FindFunctionBoundaries_WithFunctionLines) {
    auto bounds = find_function_boundaries(sample_code, function_size_warning);

    ASSERT_TRUE(bounds.has_value());
    EXPECT_EQ(bounds->first, 7);   // "void complex_function() {"
    EXPECT_EQ(bounds->second, 15); // After "}" (start_line + function_lines + 1 for after brace)
}

TEST_F(FunctionalCoreTest, FindFunctionBoundaries_NoFunctionLines) {
    Warning warning_without_lines{.file_path = "/test.cpp",
                                  .line_number = 10,
                                  .column_number = 0,
                                  .warning_type = "some-warning",
                                  .message = "test",
                                  .function_lines = std::nullopt};

    auto bounds = find_function_boundaries(sample_code, warning_without_lines);

    ASSERT_TRUE(bounds.has_value());
    EXPECT_EQ(bounds->first, 7);   // Found function start
    EXPECT_EQ(bounds->second, 15); // After closing brace (NOLINTEND placement)
}

TEST_F(FunctionalCoreTest, BuildDisplayContext_Basic) {
    auto context = build_display_context(magic_number_warning, sample_code,
                                         NolintStyle::NOLINT_SPECIFIC, 2, 2);

    EXPECT_EQ(context.warning_line_index, 2); // Warning at line 5, context starts at line 3
    EXPECT_EQ(context.format_preview, "// NOLINT(readability-magic-numbers)");
    EXPECT_THAT(context.progress_info, ::testing::HasSubstr("readability-magic-numbers"));
    EXPECT_THAT(context.progress_info, ::testing::HasSubstr("/test.cpp:5"));

    // Should have 5 lines: 3, 4, 5, 6, 7
    EXPECT_EQ(context.context_lines.size(), 5);
    EXPECT_THAT(context.context_lines[2], ::testing::HasSubstr(" >> ")); // Warning line marked
}

TEST_F(FunctionalCoreTest, BuildDisplayContext_Block_ExtendedContext) {
    auto context = build_display_context(function_size_warning, sample_code,
                                         NolintStyle::NOLINT_BLOCK, 2, 2);

    // For NOLINT_BLOCK, context should be extended to show function boundaries
    EXPECT_GT(context.context_lines.size(), 5); // More than basic context
    EXPECT_EQ(
        context.format_preview,
        "// NOLINTBEGIN(readability-function-size) ... // NOLINTEND(readability-function-size)");
}

TEST_F(FunctionalCoreTest, CombineWarningTypes_Single) {
    std::vector<std::string> types{"readability-magic-numbers"};
    auto result = combine_warning_types(types);
    EXPECT_EQ(result, "readability-magic-numbers");
}

TEST_F(FunctionalCoreTest, CombineWarningTypes_Multiple) {
    std::vector<std::string> types{"readability-magic-numbers", "readability-identifier-naming",
                                   "performance-unnecessary-copy-initialization"};
    auto result = combine_warning_types(types);
    EXPECT_EQ(result, "readability-magic-numbers,readability-identifier-naming,performance-"
                      "unnecessary-copy-initialization");
}

TEST_F(FunctionalCoreTest, CombineWarningTypes_Empty) {
    std::vector<std::string> types;
    auto result = combine_warning_types(types);
    EXPECT_EQ(result, "");
}

// Edge cases and error handling

TEST_F(FunctionalCoreTest, ApplyModification_InvalidLineNumber) {
    Modification mod{.target_line = 999, // Beyond file bounds
                     .style = NolintStyle::NOLINT_SPECIFIC,
                     .warning_type = "test-warning",
                     .indentation = "",
                     .block_start_line = std::nullopt,
                     .block_end_line = std::nullopt};

    auto result = apply_modification_to_lines(sample_code, mod);

    // Should not crash, should return original
    EXPECT_EQ(result.lines_added, 0);
    EXPECT_EQ(result.lines, sample_code);
}

TEST_F(FunctionalCoreTest, CreateModification_InvalidLineIndex) {
    Warning invalid_warning{.file_path = "/test.cpp",
                            .line_number = 999, // Beyond bounds
                            .column_number = 0,
                            .warning_type = "test-warning",
                            .message = "test message",
                            .function_lines = std::nullopt};

    auto mod = create_modification(invalid_warning, NolintStyle::NOLINT_SPECIFIC, sample_code);

    EXPECT_EQ(mod.target_line, 999);
    EXPECT_EQ(mod.style, NolintStyle::NOLINT_SPECIFIC);
    EXPECT_TRUE(mod.indentation.empty()); // No indentation extracted
}

TEST_F(FunctionalCoreTest, FindFunctionBoundaries_InvalidWarningLine) {
    Warning invalid_warning{.file_path = "/test.cpp",
                            .line_number = -1,
                            .column_number = 0,
                            .warning_type = "test-warning",
                            .message = "test message",
                            .function_lines = std::nullopt};

    auto bounds = find_function_boundaries(sample_code, invalid_warning);
    EXPECT_FALSE(bounds.has_value());
}

// Test realistic scenarios combining multiple operations - this is key for integration

TEST_F(FunctionalCoreTest, RealisticScenario_MagicNumberSuppression) {
    // User cycles from NONE -> SPECIFIC -> NEXTLINE -> NONE and chooses SPECIFIC
    Warning warning = magic_number_warning;

    // Test the full cycle
    NolintStyle style = NolintStyle::NONE;
    style = cycle_nolint_style(style, false); // -> NOLINT_SPECIFIC
    EXPECT_EQ(style, NolintStyle::NOLINT_SPECIFIC);

    style = cycle_nolint_style(style, false); // -> NOLINTNEXTLINE
    EXPECT_EQ(style, NolintStyle::NOLINTNEXTLINE);

    style = cycle_nolint_style(style, false); // -> NONE
    EXPECT_EQ(style, NolintStyle::NONE);

    // User chooses NOLINT_SPECIFIC
    style = cycle_nolint_style(style, false); // -> NOLINT_SPECIFIC

    auto mod = create_modification(warning, style, sample_code);
    auto result = apply_modification_to_lines(sample_code, mod);

    EXPECT_THAT(result.lines[4], ::testing::HasSubstr("// NOLINT(readability-magic-numbers)"));
}

TEST_F(FunctionalCoreTest, RealisticScenario_FunctionSizeBlock) {
    // Test NOLINT_BLOCK for function size warning
    Warning warning = function_size_warning;

    auto mod = create_modification(warning, NolintStyle::NOLINT_BLOCK, sample_code);
    auto result = apply_modification_to_lines(sample_code, mod);

    // Verify NOLINTBEGIN/END placement
    bool found_begin = false, found_end = false;
    for (const auto& line : result.lines) {
        if (line.find("NOLINTBEGIN(readability-function-size)") != std::string::npos) {
            found_begin = true;
        }
        if (line.find("NOLINTEND(readability-function-size)") != std::string::npos) {
            found_end = true;
        }
    }

    EXPECT_TRUE(found_begin);
    EXPECT_TRUE(found_end);
    EXPECT_EQ(result.lines_added, 2);
}

// Phase 1: Core Filter Logic Tests (TDD Approach)
//
// These tests define the expected behavior of the search/filter functionality
// before implementation. This tests the pure filtering functions that will
// be used for the '/' search feature.

class FilterTest : public ::testing::Test {
protected:
    // Test warnings with different characteristics for filtering
    const std::vector<Warning> test_warnings = {
        Warning{.file_path = "/src/main.cpp",
                .line_number = 42,
                .column_number = 10,
                .warning_type = "readability-magic-numbers",
                .message = "42 is a magic number; consider replacing it with a named constant",
                .function_lines = std::nullopt},

        Warning{.file_path = "/src/parser.cpp",
                .line_number = 123,
                .column_number = 5,
                .warning_type = "readability-function-size",
                .message = "function 'parseExpression' is too long (45 lines)",
                .function_lines = 45},

        Warning{.file_path = "/include/utils.hpp",
                .line_number = 67,
                .column_number = 15,
                .warning_type = "performance-unnecessary-copy-initialization",
                .message = "variable 'result' is copied unnecessarily; consider using a reference",
                .function_lines = std::nullopt},

        Warning{.file_path = "/tests/test_main.cpp",
                .line_number = 89,
                .column_number = 20,
                .warning_type = "readability-identifier-naming",
                .message = "variable 'testVar' is not named according to convention",
                .function_lines = std::nullopt},

        Warning{.file_path = "/src/MAIN.CPP", // Different case for testing
                .line_number = 100,
                .column_number = 1,
                .warning_type = "bugprone-unused-parameter",
                .message = "parameter 'unused_param' is unused",
                .function_lines = std::nullopt}};
};

// Test 1: Empty filter shows all warnings
TEST_F(FilterTest, EmptyFilter_ShowsAllWarnings) {
    std::string filter = "";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    EXPECT_EQ(filtered.size(), test_warnings.size());
    EXPECT_EQ(filtered, test_warnings);
}

// Test 2: Filter by warning type (exact match)
TEST_F(FilterTest, FilterByWarningType_ExactMatch) {
    std::string filter = "readability-magic-numbers";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].warning_type, "readability-magic-numbers");
    EXPECT_EQ(filtered[0].file_path, "/src/main.cpp");
}

// Test 3: Filter by warning type (partial match)
TEST_F(FilterTest, FilterByWarningType_PartialMatch) {
    std::string filter = "readability";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match readability-magic-numbers, readability-function-size,
    // readability-identifier-naming
    EXPECT_EQ(filtered.size(), 3);

    for (const auto& warning : filtered) {
        EXPECT_THAT(warning.warning_type, ::testing::HasSubstr("readability"));
    }
}

// Test 4: Filter by file path (exact match)
TEST_F(FilterTest, FilterByFilePath_ExactMatch) {
    std::string filter = "readability-magic-numbers"; // This should only match one warning
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].file_path, "/src/main.cpp");
    EXPECT_EQ(filtered[0].warning_type, "readability-magic-numbers");
}

// Test 5: Filter by file path (partial match)
TEST_F(FilterTest, FilterByFilePath_PartialMatch) {
    std::string filter = "main";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match /src/main.cpp, /tests/test_main.cpp, and /src/MAIN.CPP (case insensitive)
    EXPECT_EQ(filtered.size(), 3);

    for (const auto& warning : filtered) {
        std::string lowercase_path = warning.file_path;
        std::transform(lowercase_path.begin(), lowercase_path.end(), lowercase_path.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        EXPECT_THAT(lowercase_path, ::testing::HasSubstr("main"));
    }
}

// Test 6: Filter by message text
TEST_F(FilterTest, FilterByMessage_PartialMatch) {
    std::string filter = "magic number";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    ASSERT_EQ(filtered.size(), 1);
    EXPECT_THAT(filtered[0].message, ::testing::HasSubstr("magic number"));
}

// Test 7: Case insensitive matching
TEST_F(FilterTest, FilterCaseInsensitive) {
    std::string filter = "MAIN";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match main.cpp, MAIN.CPP, and test_main.cpp (all contain "main" case-insensitively)
    EXPECT_EQ(filtered.size(), 3);

    // Verify all contain "main" when compared case-insensitively
    for (const auto& warning : filtered) {
        std::string lowercase_path = warning.file_path;
        std::transform(lowercase_path.begin(), lowercase_path.end(), lowercase_path.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        EXPECT_THAT(lowercase_path, ::testing::HasSubstr("main"));
    }
}

// Test 8: No matches returns empty vector
TEST_F(FilterTest, NoMatches_ReturnsEmpty) {
    std::string filter = "nonexistent-pattern";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    EXPECT_TRUE(filtered.empty());
}

// Test 9: Whitespace handling
TEST_F(FilterTest, FilterWithWhitespace_TrimmedAndMatched) {
    std::string filter = "  readability  ";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should still match readability warnings despite whitespace
    EXPECT_EQ(filtered.size(), 3);
}

// Test 10: Multiple terms with AND logic
TEST_F(FilterTest, MultipleTerms_ANDLogic) {
    std::string filter = "readability main";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match warnings that contain BOTH "readability" AND "main"
    // This matches: /src/main.cpp with readability-magic-numbers
    //           and: /tests/test_main.cpp with readability-identifier-naming
    ASSERT_EQ(filtered.size(), 2);

    for (const auto& warning : filtered) {
        EXPECT_THAT(warning.warning_type, ::testing::HasSubstr("readability"));
        std::string lowercase_path = warning.file_path;
        std::transform(lowercase_path.begin(), lowercase_path.end(), lowercase_path.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        EXPECT_THAT(lowercase_path, ::testing::HasSubstr("main"));
    }
}

// Test 11: Complex filter with file extension
TEST_F(FilterTest, FilterByFileExtension) {
    std::string filter = ".hpp";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].file_path, "/include/utils.hpp");
}

// Test 12: Filter preserves order
TEST_F(FilterTest, FilterPreservesOriginalOrder) {
    std::string filter = "src";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match warnings in /src/ directory in original order
    EXPECT_GE(filtered.size(), 2);

    // Verify order is preserved (main.cpp comes before parser.cpp in test_warnings)
    bool found_main = false;
    size_t main_index = 0, parser_index = 0;

    for (size_t i = 0; i < filtered.size(); ++i) {
        if (filtered[i].file_path == "/src/main.cpp") {
            found_main = true;
            main_index = i;
        }
        if (filtered[i].file_path == "/src/parser.cpp") {
            parser_index = i;
        }
    }

    if (found_main && parser_index > 0) {
        EXPECT_LT(main_index, parser_index);
    }
}

// Test 13: Edge case - single character filter
TEST_F(FilterTest, SingleCharacterFilter) {
    std::string filter = "p";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match any warning containing 'p' (parser.cpp, performance-, param, etc.)
    EXPECT_GT(filtered.size(), 0);

    for (const auto& warning : filtered) {
        bool contains_p = (warning.file_path.find('p') != std::string::npos)
                          || (warning.warning_type.find('p') != std::string::npos)
                          || (warning.message.find('p') != std::string::npos);
        EXPECT_TRUE(contains_p) << "Warning should contain 'p': " << warning.file_path;
    }
}

// Test 14: Numeric patterns in filters
TEST_F(FilterTest, NumericPatternFilter) {
    std::string filter = "42";
    auto filtered = functional_core::filter_warnings(test_warnings, filter);

    // Should match the line number 42 from main.cpp and "42 is a magic number" message
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].line_number, 42);
    EXPECT_THAT(filtered[0].message, ::testing::HasSubstr("42"));
}

// Test 15: Filter performance with large warning sets
TEST_F(FilterTest, FilterPerformance_LargeWarningSet) {
    // Create a larger test set
    std::vector<Warning> large_warnings;
    for (int i = 0; i < 1000; ++i) {
        large_warnings.push_back(Warning{.file_path = "/src/file" + std::to_string(i) + ".cpp",
                                         .line_number = i + 1,
                                         .column_number = 10,
                                         .warning_type = (i % 2 == 0) ? "readability-magic-numbers"
                                                                      : "performance-issue",
                                         .message = "Warning message " + std::to_string(i),
                                         .function_lines = std::nullopt});
    }

    std::string filter = "readability";
    auto start = std::chrono::high_resolution_clock::now();
    auto filtered = functional_core::filter_warnings(large_warnings, filter);
    auto end = std::chrono::high_resolution_clock::now();

    // Should complete quickly and filter correctly
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100); // Should complete in under 100ms
    EXPECT_EQ(filtered.size(), 500);  // Half should match "readability"
}