#include "functional_core.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

namespace nolint::functional_core {

auto create_modification(const Warning& warning, NolintStyle style,
                         std::span<const std::string> file_lines) -> Modification {
    if (style == NolintStyle::NONE) {
        return Modification{.target_line = warning.line_number,
                            .style = NolintStyle::NONE,
                            .warning_type = warning.warning_type,
                            .indentation = "",
                            .block_start_line = std::nullopt,
                            .block_end_line = std::nullopt};
    }

    // Get indentation from the warning line (1-based to 0-based)
    const int line_index = warning.line_number - 1;
    std::string indentation;
    if (line_index >= 0 && line_index < static_cast<int>(file_lines.size())) {
        indentation = std::string(extract_indentation(file_lines[line_index]));
    }

    Modification mod{.target_line = warning.line_number,
                     .style = style,
                     .warning_type = warning.warning_type,
                     .indentation = indentation,
                     .block_start_line = std::nullopt,
                     .block_end_line = std::nullopt};

    // For NOLINT_BLOCK, find function boundaries
    if (style == NolintStyle::NOLINT_BLOCK && warning.function_lines.has_value()) {
        if (auto bounds = find_function_boundaries(file_lines, warning)) {
            mod.block_start_line = bounds->first;
            mod.block_end_line = bounds->second;
        } else {
            // Fallback: use warning line +/- 5 lines
            mod.block_start_line = std::max(1, warning.line_number - 5);
            mod.block_end_line
                = std::min(static_cast<int>(file_lines.size()), warning.line_number + 5);
        }
    }

    return mod;
}

auto apply_modification_to_lines(std::span<const std::string> original_lines,
                                 const Modification& modification) -> TextTransformation {
    if (modification.style == NolintStyle::NONE) {
        // No modification - return original
        return TextTransformation{
            .lines = std::vector<std::string>(original_lines.begin(), original_lines.end()),
            .lines_added = 0,
            .lines_removed = 0};
    }

    auto comments = format_nolint_comment(modification.style, modification.warning_type,
                                          modification.indentation);

    std::vector<std::string> result(original_lines.begin(), original_lines.end());
    int lines_added = 0;

    switch (modification.style) {
    case NolintStyle::NOLINT_SPECIFIC: {
        // Add inline comment to the target line
        const int line_index = modification.target_line - 1;
        if (line_index >= 0 && line_index < static_cast<int>(result.size())) {
            result[line_index] += "  " + comments[0];
        }
        break;
    }

    case NolintStyle::NOLINTNEXTLINE: {
        // Insert comment before the target line
        const int insert_index = modification.target_line - 1;
        if (insert_index >= 0 && insert_index <= static_cast<int>(result.size())) {
            result.insert(result.begin() + insert_index, comments[0]);
            lines_added = 1;
        }
        break;
    }

    case NolintStyle::NOLINT_BLOCK: {
        if (modification.block_start_line && modification.block_end_line) {
            // Insert NOLINTBEGIN before start line (1-based to 0-based)
            const int start_index = modification.block_start_line.value() - 1;
            if (start_index >= 0 && start_index <= static_cast<int>(result.size())) {
                result.insert(result.begin() + start_index, comments[0]);
                lines_added++;

                // Insert NOLINTEND after end line (adjusted for NOLINTBEGIN insertion)
                const int end_index
                    = modification.block_end_line.value()
                      + 1; // +1 to go after end line, accounting for NOLINTBEGIN shift
                if (end_index >= 0 && end_index <= static_cast<int>(result.size())) {
                    result.insert(result.begin() + end_index, comments[1]);
                    lines_added++;
                }
            }
        }
        break;
    }

    case NolintStyle::NONE:
        // Already handled above
        break;
    }

    return TextTransformation{
        .lines = std::move(result), .lines_added = lines_added, .lines_removed = 0};
}

auto apply_modifications_to_lines(std::span<const std::string> original_lines,
                                  std::span<const Modification> modifications)
    -> TextTransformation {
    TextTransformation result{
        .lines = std::vector<std::string>(original_lines.begin(), original_lines.end()),
        .lines_added = 0,
        .lines_removed = 0};

    // Sort modifications by line number (descending) to avoid line number shifts
    std::vector<Modification> sorted_mods(modifications.begin(), modifications.end());
    std::ranges::sort(sorted_mods,
                      [](const auto& a, const auto& b) { return a.target_line > b.target_line; });

    for (const auto& mod : sorted_mods) {
        auto transformation = apply_modification_to_lines(result.lines, mod);
        result.lines = std::move(transformation.lines);
        result.lines_added += transformation.lines_added;
        result.lines_removed += transformation.lines_removed;
    }

    return result;
}

auto format_nolint_comment(NolintStyle style, std::string_view warning_type,
                           std::string_view indentation) -> std::vector<std::string> {
    using namespace std::string_literals;

    switch (style) {
    case NolintStyle::NOLINT_SPECIFIC:
        return {"// NOLINT(" + std::string(warning_type) + ")"};

    case NolintStyle::NOLINTNEXTLINE:
        return {std::string(indentation) + "// NOLINTNEXTLINE(" + std::string(warning_type) + ")"};

    case NolintStyle::NOLINT_BLOCK:
        return {std::string(indentation) + "// NOLINTBEGIN(" + std::string(warning_type) + ")",
                std::string(indentation) + "// NOLINTEND(" + std::string(warning_type) + ")"};

    case NolintStyle::NONE:
        return {};
    }

    return {};
}

auto find_function_boundaries(std::span<const std::string> file_lines, const Warning& warning)
    -> std::optional<std::pair<int, int>> {
    const int warning_line = warning.line_number - 1; // Convert to 0-based

    if (warning_line < 0 || warning_line >= static_cast<int>(file_lines.size())) {
        return std::nullopt;
    }

    // Search backwards for function signature
    int start_line = warning_line;
    for (int i = warning_line; i >= 0; --i) {
        if (is_function_signature(file_lines[i])) {
            start_line = i;
            break;
        }
        // Don't search too far back
        if (warning_line - i > 20)
            break;
    }

    // Use function_lines hint if available, otherwise search for closing brace
    int end_line = warning_line;
    if (warning.function_lines.has_value()) {
        // Use function_lines hint: count from the warning line (function signature)
        // SAFETY: Limit function size to prevent memory exhaustion
        int safe_function_lines = std::min(warning.function_lines.value(), 1000);
        end_line = std::min(warning_line + safe_function_lines - 1,
                            static_cast<int>(file_lines.size()) - 1);
    } else {
        // Simple heuristic: find closing brace that matches function indentation level
        const auto start_indent = extract_indentation(file_lines[start_line]);
        for (int i = warning_line + 1; i < static_cast<int>(file_lines.size()); ++i) {
            const auto line = file_lines[i];
            const auto line_indent = extract_indentation(line);
            // Look for closing brace with same indentation as function signature
            if (line_indent.size() == start_indent.size() && line.find('}') != std::string::npos) {
                end_line = i + 1; // NOLINTEND should go after the closing brace
                break;
            }
            // Don't search too far forward
            if (i - warning_line > 50)
                break;
        }
    }

    return std::make_pair(start_line + 1, end_line + 1); // Convert back to 1-based
}

auto build_display_context(const Warning& warning, std::span<const std::string> file_lines,
                           NolintStyle current_style, int context_before, int context_after)
    -> DisplayContext {
    const int warning_line = warning.line_number - 1; // Convert to 0-based
    const int total_lines = static_cast<int>(file_lines.size());

    // For NOLINT_BLOCK, show extended context
    if (current_style == NolintStyle::NOLINT_BLOCK) {
        auto mod = create_modification(warning, current_style, file_lines);
        if (mod.block_start_line && mod.block_end_line) {
            context_before
                = std::max(context_before, warning_line - (mod.block_start_line.value() - 1));
            int calculated_context_after = mod.block_end_line.value() - warning_line;
            // SAFETY: Limit context size to prevent memory exhaustion
            context_after = std::max(context_after, std::min(calculated_context_after, 100));
        }
    }

    const int start_line = std::max(0, warning_line - context_before);

    // Apply the current style to get the transformed code for preview
    std::vector<std::string> display_lines;
    if (current_style != NolintStyle::NONE) {
        auto mod = create_modification(warning, current_style, file_lines);
        if (mod.style != NolintStyle::NONE) {
            auto transformation = apply_modification_to_lines(file_lines, mod);
            display_lines = std::move(transformation.lines);
        } else {
            display_lines = std::vector<std::string>(file_lines.begin(), file_lines.end());
        }
    } else {
        display_lines = std::vector<std::string>(file_lines.begin(), file_lines.end());
    }

    // Adjust end_line if transformation added lines
    const int display_total_lines = static_cast<int>(display_lines.size());
    const int adjusted_end_line
        = std::min(display_total_lines - 1,
                   warning_line + context_after
                       + (display_total_lines - total_lines)); // Account for added lines

    std::vector<std::string> context_lines;

    // For NOLINT_BLOCK, use truncated display to avoid overwhelming output
    if (current_style == NolintStyle::NOLINT_BLOCK && (adjusted_end_line - start_line) > 12) {
        // Show first few lines (including NOLINTBEGIN)
        for (int i = start_line; i <= start_line + 5 && i <= adjusted_end_line; ++i) {
            if (i >= display_total_lines)
                break;

            std::ostringstream line_num_stream;
            int display_line_num = i + 1;
            line_num_stream << std::setw(4) << display_line_num;
            const std::string line_num = line_num_stream.str();
            const std::string marker = (i == warning_line) ? " >> " : "    ";
            context_lines.push_back(marker + line_num + "| "
                                    + highlight_nolint_comments(display_lines[i]));
        }

        // Show truncation indicator
        int skipped_lines = adjusted_end_line - (start_line + 5) - 5;
        if (skipped_lines > 0) {
            context_lines.push_back("    ... | (" + std::to_string(skipped_lines)
                                    + " lines skipped)");
        }

        // Show last few lines (including NOLINTEND)
        for (int i = std::max(start_line + 6, adjusted_end_line - 4); i <= adjusted_end_line; ++i) {
            if (i >= display_total_lines)
                break;

            std::ostringstream line_num_stream;
            int display_line_num = i + 1;
            line_num_stream << std::setw(4) << display_line_num;
            const std::string line_num = line_num_stream.str();
            const std::string marker = (i == warning_line) ? " >> " : "    ";
            context_lines.push_back(marker + line_num + "| "
                                    + highlight_nolint_comments(display_lines[i]));
        }
    } else {
        // Normal display for other styles or short NOLINT_BLOCK
        for (int i = start_line; i <= adjusted_end_line; ++i) {
            if (i >= display_total_lines)
                break;

            std::ostringstream line_num_stream;
            // For added lines (NOLINTNEXTLINE), show original line numbers
            int display_line_num
                = (current_style == NolintStyle::NOLINTNEXTLINE && i > warning_line) ? i : i + 1;
            if (current_style == NolintStyle::NOLINTNEXTLINE && i == warning_line) {
                // This is the added NOLINTNEXTLINE, show with + marker and green highlighting
                context_lines.push_back(" +      | " + highlight_nolint_comments(display_lines[i]));
            } else {
                line_num_stream << std::setw(4) << display_line_num;
                const std::string line_num = line_num_stream.str();
                const std::string marker
                    = (i == warning_line
                       || (current_style == NolintStyle::NOLINTNEXTLINE && i == warning_line + 1))
                          ? " >> "
                          : "    ";
                context_lines.push_back(marker + line_num + "| "
                                        + highlight_nolint_comments(display_lines[i]));
            }
        }
    }

    // Simple format description for the style being applied
    std::string format_preview;
    switch (current_style) {
    case NolintStyle::NONE:
        format_preview = "No suppression";
        break;
    case NolintStyle::NOLINT_SPECIFIC:
        format_preview = "// NOLINT(" + warning.warning_type + ")";
        break;
    case NolintStyle::NOLINTNEXTLINE:
        format_preview = "// NOLINTNEXTLINE(" + warning.warning_type + ")";
        break;
    case NolintStyle::NOLINT_BLOCK:
        format_preview = "// NOLINTBEGIN(" + warning.warning_type + ") ... // NOLINTEND("
                         + warning.warning_type + ")";
        break;
    }

    std::string progress_info = "Processing " + warning.warning_type + " in " + warning.file_path
                                + ":" + std::to_string(warning.line_number);

    return DisplayContext{.context_lines = std::move(context_lines),
                          .warning_line_index = warning_line - start_line,
                          .format_preview = std::move(format_preview),
                          .progress_info = std::move(progress_info)};
}

auto combine_warning_types(std::span<const std::string> warning_types) -> std::string {
    if (warning_types.empty())
        return "";
    if (warning_types.size() == 1)
        return warning_types[0];

    std::ostringstream oss;
    oss << warning_types[0];
    for (size_t i = 1; i < warning_types.size(); ++i) {
        oss << "," << warning_types[i];
    }
    return oss.str();
}

auto highlight_nolint_comments(const std::string& line) -> std::string {
    // ANSI color codes
    const std::string GREEN = "\033[32m";
    const std::string RESET = "\033[0m";

    // First, strip any existing ANSI color codes to prevent double-highlighting
    std::string result = line;
    size_t ansi_pos = 0;
    while ((ansi_pos = result.find("\033[", ansi_pos)) != std::string::npos) {
        size_t end_pos = result.find('m', ansi_pos);
        if (end_pos != std::string::npos) {
            result.erase(ansi_pos, end_pos - ansi_pos + 1);
        } else {
            // Malformed ANSI code, just skip this position
            ansi_pos++;
        }
    }

    // Find and highlight NOLINT comments
    std::vector<std::string> nolint_patterns
        = {"// NOLINT(", "// NOLINTNEXTLINE(", "// NOLINTBEGIN(", "// NOLINTEND(",
           "// NOLINT)", "// NOLINTNEXTLINE)", "// NOLINTBEGIN)", "// NOLINTEND)"};

    for (const auto& pattern : nolint_patterns) {
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != std::string::npos) {
            // Find the end of the comment (end of line or start of another comment)
            size_t end_pos = result.find('\n', pos);
            if (end_pos == std::string::npos) {
                end_pos = result.length();
            }

            // Find the closing parenthesis for the NOLINT comment
            size_t close_paren = result.find(')', pos);
            if (close_paren != std::string::npos && close_paren < end_pos) {
                end_pos = close_paren + 1;
            }

            // Insert color codes - insert at the END first to avoid position shifts
            result.insert(end_pos, RESET);
            result.insert(pos, GREEN);

            // Adjust position for next search - need to account for the GREEN insertion
            // that shifted end_pos to the right
            pos = end_pos + GREEN.length() + RESET.length();

            // Safety check to prevent infinite loops - ensure we always advance
            if (pos <= end_pos) {
                pos = end_pos + 1;
            }
            if (pos >= result.length()) {
                break;
            }
        }
    }

    return result;
}

auto filter_warnings(std::span<const Warning> warnings, std::string_view filter)
    -> std::vector<Warning> {

    std::vector<Warning> filtered_warnings;

    // Empty filter means show all warnings
    if (filter.empty()) {
        filtered_warnings.assign(warnings.begin(), warnings.end());
        return filtered_warnings;
    }

    // Trim whitespace from filter
    auto trim_left = [](std::string_view str) {
        const size_t start = str.find_first_not_of(" \t\n\r");
        return (start == std::string_view::npos) ? std::string_view{} : str.substr(start);
    };

    auto trim_right = [](std::string_view str) {
        const size_t end = str.find_last_not_of(" \t\n\r");
        return (end == std::string_view::npos) ? std::string_view{} : str.substr(0, end + 1);
    };

    auto trim = [&](std::string_view str) { return trim_right(trim_left(str)); };

    std::string_view trimmed_filter = trim(filter);
    if (trimmed_filter.empty()) {
        filtered_warnings.assign(warnings.begin(), warnings.end());
        return filtered_warnings;
    }

    // Convert filter to lowercase for case-insensitive matching
    std::string lowercase_filter{trimmed_filter};
    std::transform(lowercase_filter.begin(), lowercase_filter.end(), lowercase_filter.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Split filter into terms for AND logic
    std::vector<std::string> filter_terms;
    std::istringstream iss(lowercase_filter);
    std::string term;
    while (iss >> term) {
        filter_terms.push_back(term);
    }

    // If no terms after splitting, show all
    if (filter_terms.empty()) {
        filtered_warnings.assign(warnings.begin(), warnings.end());
        return filtered_warnings;
    }

    // Helper to convert string to lowercase
    auto to_lowercase = [](const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    };

    // Filter warnings that match ALL terms (AND logic)
    for (size_t warn_idx = 0; warn_idx < warnings.size(); ++warn_idx) {
        const auto& warning = warnings[warn_idx];

        // Check if ALL filter terms match this warning
        bool all_terms_match = true;
        for (const auto& filter_term : filter_terms) {
            // MEMORY OPTIMIZATION: Convert each field once, not multiple times
            std::string lower_file_path = to_lowercase(warning.file_path);
            std::string lower_warning_type = to_lowercase(warning.warning_type);
            std::string lower_message = to_lowercase(warning.message);
            std::string line_number_str = std::to_string(warning.line_number);

            // Check each field separately for a match
            bool term_matches = lower_file_path.find(filter_term) != std::string::npos
                                || lower_warning_type.find(filter_term) != std::string::npos
                                || lower_message.find(filter_term) != std::string::npos
                                || line_number_str.find(filter_term) != std::string::npos;

            if (!term_matches) {
                all_terms_match = false;
                break;
            }
        }

        if (all_terms_match) {
            filtered_warnings.push_back(warning);
        }
    }

    return filtered_warnings;
}

} // namespace nolint::functional_core