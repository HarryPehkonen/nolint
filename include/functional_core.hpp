#pragma once

#include "nolint_types.hpp"
#include <ranges>
#include <span>
#include <string_view>

namespace nolint::functional_core {

// Pure text transformation functions - no I/O, easy to test

/**
 * Extract indentation (spaces/tabs) from the beginning of a line
 */
inline auto extract_indentation(std::string_view line) -> std::string_view {
    const auto first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string_view::npos) {
        return line; // Entire line is whitespace
    }
    return line.substr(0, first_non_space);
}

/**
 * Create a modification instruction based on warning and desired style
 */
auto create_modification(const Warning& warning, NolintStyle style,
                         std::span<const std::string> file_lines) -> Modification;

/**
 * Apply a single modification to lines, returning transformed result
 * This is a pure function - no side effects
 */
auto apply_modification_to_lines(std::span<const std::string> original_lines,
                                 const Modification& modification) -> TextTransformation;

/**
 * Apply multiple modifications in sequence
 * Handles line number adjustments automatically
 */
auto apply_modifications_to_lines(std::span<const std::string> original_lines,
                                  std::span<const Modification> modifications)
    -> TextTransformation;

/**
 * Format a NOLINT comment based on style and warning type
 */
auto format_nolint_comment(NolintStyle style, std::string_view warning_type,
                           std::string_view indentation = "") -> std::vector<std::string>;

/**
 * Find function boundaries for NOLINT_BLOCK placement
 * Returns {start_line, end_line} or nullopt if not found
 */
auto find_function_boundaries(std::span<const std::string> file_lines, const Warning& warning)
    -> std::optional<std::pair<int, int>>;

/**
 * Build display context showing code around warning with preview
 */
auto build_display_context(const Warning& warning, std::span<const std::string> file_lines,
                           NolintStyle current_style, int context_before = 5, int context_after = 5)
    -> DisplayContext;

/**
 * Get next style in cycling order
 * NONE → NOLINT_SPECIFIC → NOLINTNEXTLINE → NOLINT_BLOCK (if applicable) → NONE
 */
inline auto cycle_nolint_style(NolintStyle current, bool block_available) -> NolintStyle {
    switch (current) {
    case NolintStyle::NONE:
        return NolintStyle::NOLINT_SPECIFIC;
    case NolintStyle::NOLINT_SPECIFIC:
        return NolintStyle::NOLINTNEXTLINE;
    case NolintStyle::NOLINTNEXTLINE:
        return block_available ? NolintStyle::NOLINT_BLOCK : NolintStyle::NONE;
    case NolintStyle::NOLINT_BLOCK:
        return NolintStyle::NONE;
    }
    return NolintStyle::NONE;
}

/**
 * Check if a line looks like a function signature
 */
inline auto is_function_signature(std::string_view line) -> bool {
    // Simple heuristic: contains '(' and ')' and looks like function
    // More sophisticated parsing would require AST
    const bool has_parens
        = line.find('(') != std::string_view::npos && line.find(')') != std::string_view::npos;
    const bool not_comment = !line.starts_with("//") && !line.starts_with("/*");
    const bool not_if_while = line.find("if ") == std::string_view::npos
                              && line.find("while ") == std::string_view::npos
                              && line.find("for ") == std::string_view::npos;

    return has_parens && not_comment && not_if_while;
}

/**
 * Combine multiple warning types into a single NOLINT comment
 * E.g., "magic-numbers,readability-identifier-naming"
 */
auto combine_warning_types(std::span<const std::string> warning_types) -> std::string;

/**
 * Add green color highlighting to NOLINT comments in a line
 */
auto highlight_nolint_comments(const std::string& line) -> std::string;

/**
 * Filter warnings based on search criteria
 * Searches file path, warning type, and message text (case-insensitive)
 * Multiple terms use AND logic (all terms must match)
 * Returns filtered warnings preserving original order
 */
auto filter_warnings(std::span<const Warning> warnings, std::string_view filter)
    -> std::vector<Warning>;

} // namespace nolint::functional_core