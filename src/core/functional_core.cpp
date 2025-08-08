#include "nolint/core/functional_core.hpp"
#include <algorithm>
#include <numeric>
#include <ranges>  // For C++20 ranges
#include <sstream>
#include <unordered_map>
#include <vector>

namespace nolint::functional_core {

auto apply_decision(AnnotatedFile& file, const Warning& warning, NolintStyle style) -> void {
    if (warning.line_number == 0 || warning.line_number > file.lines.size()) {
        return; // Invalid line number
    }

    size_t line_index = warning.line_number - 1; // Convert to 0-based index
    std::string indent = extract_indentation(file.lines[line_index].text);

    switch (style) {
    case NolintStyle::NONE:
        // No suppression applied
        break;

    case NolintStyle::NOLINT_SPECIFIC:
        file.lines[line_index].inline_comment = "// NOLINT(" + warning.warning_type + ")";
        break;

    case NolintStyle::NOLINTNEXTLINE:
        file.lines[line_index].before_comments.push_back(indent + "// NOLINTNEXTLINE("
                                                         + warning.warning_type + ")");
        break;

    case NolintStyle::NOLINT_BLOCK:
        if (warning.function_lines.has_value()) {
            // OPTIMIZED: Use C++20 ranges to avoid copying - create lazy view
            auto line_texts = file.lines | std::views::transform(&AnnotatedLine::text);
            
            auto [start, end] = find_function_boundaries(line_texts, warning);
            file.blocks.push_back(
                {.start_line = start, .end_line = end, .warning_type = warning.warning_type});
        }
        break;
    }
}

// REMOVED: Template definition moved to header file for proper visibility

auto filter_warnings(std::span<const Warning> warnings, std::string_view filter_terms)
    -> std::vector<size_t> {
    if (filter_terms.empty()) {
        std::vector<size_t> all_indices(warnings.size());
        std::iota(all_indices.begin(), all_indices.end(), 0);
        return all_indices;
    }

    auto terms = split_by_whitespace(to_lowercase(trim(filter_terms)));
    std::vector<size_t> filtered_indices;

    for (size_t i = 0; i < warnings.size(); ++i) {
        const auto& warning = warnings[i];
        bool all_terms_match = true;

        // Search across all fields
        std::string lower_file_path = to_lowercase(warning.file_path);
        std::string lower_warning_type = to_lowercase(warning.warning_type);
        std::string lower_message = to_lowercase(warning.message);
        std::string line_number_str = std::to_string(warning.line_number);

        for (const auto& term : terms) {
            bool term_matches = lower_file_path.find(term) != std::string::npos
                                || lower_warning_type.find(term) != std::string::npos
                                || lower_message.find(term) != std::string::npos
                                || line_number_str.find(term) != std::string::npos;

            if (!term_matches) {
                all_terms_match = false;
                break;
            }
        }

        if (all_terms_match) {
            filtered_indices.push_back(i);
        }
    }

    return filtered_indices;
}

auto split_by_whitespace(std::string_view text) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    std::istringstream iss{std::string(text)};
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

auto to_lowercase(std::string_view text) -> std::string {
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

auto trim(std::string_view text) -> std::string {
    auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

auto calculate_statistics(const std::vector<Warning>& warnings, const Decisions& decisions,
                          const std::unordered_set<std::string>& visited_warnings)
    -> std::vector<WarningTypeStats> {
    std::unordered_map<std::string, WarningTypeStats> stats_map;

    for (const auto& warning : warnings) {
        auto& stats = stats_map[warning.warning_type];
        stats.warning_type = warning.warning_type;
        stats.total_count++;

        // Check if addressed (has NOLINT suppression applied)
        auto warning_key_str = warning_key(warning);
        auto decision_it = decisions.find(warning_key_str);
        if (decision_it != decisions.end() && decision_it->second != NolintStyle::NONE) {
            stats.addressed_count++;
        }

        // Check if visited (displayed to user)
        if (visited_warnings.contains(warning_key_str)) {
            stats.visited_count++;
        }
    }

    // Convert to sorted vector
    std::vector<WarningTypeStats> result;
    result.reserve(stats_map.size());

    for (const auto& [type, stats] : stats_map) {
        result.push_back(stats);
    }

    // Sort alphabetically by warning type
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.warning_type < b.warning_type; });

    return result;
}

auto build_display_context(const Warning& warning, const std::vector<std::string>& file_lines,
                           NolintStyle current_style) -> DisplayContext {
    DisplayContext context;

    if (file_lines.empty() || warning.line_number == 0 || warning.line_number > file_lines.size()) {
        return context;
    }

    // Build context around the warning line (±5 lines)
    constexpr size_t context_size = 5;
    size_t warning_index = warning.line_number - 1;
    size_t start_line = warning_index >= context_size ? warning_index - context_size : 0;
    size_t end_line
        = std::min(warning_index + context_size + 1, static_cast<size_t>(file_lines.size()));

    // Extract context lines with preview of modifications
    for (size_t i = start_line; i < end_line; ++i) {
        std::string line = file_lines[i];
        bool is_warning_line = (i == warning_index);

        // Apply preview modifications for warning line
        if (is_warning_line && current_style != NolintStyle::NONE) {
            std::string indent = extract_indentation(line);

            switch (current_style) {
            case NolintStyle::NOLINT_SPECIFIC:
                line += "  // NOLINT(" + warning.warning_type + ")";
                break;

            case NolintStyle::NOLINTNEXTLINE: {
                // Add before comment with highlighting
                std::string preview_line = indent + "// NOLINTNEXTLINE(" + warning.warning_type + ")";
                context.context_lines.push_back("+" + std::string(8, ' ') + "| " + highlight_nolint_comments(preview_line));
                break;
            }

            case NolintStyle::NOLINT_BLOCK: {
                if (warning.function_lines.has_value()) {
                    // Show block suppression preview with highlighting
                    std::string preview_line = indent + "// NOLINTBEGIN(" + warning.warning_type + ")";
                    context.context_lines.push_back("+" + std::string(8, ' ') + "| " + highlight_nolint_comments(preview_line));
                }
                break;
            }

            case NolintStyle::NONE:
                break;
            }
        }

        // Add the actual line with conditional highlighting
        std::string prefix = is_warning_line ? ">>" : "  ";
        std::string line_num = std::to_string(i + 1);
        
        // Only highlight if this is the warning line AND it has an inline NOLINT added
        std::string display_line;
        if (is_warning_line && current_style == NolintStyle::NOLINT_SPECIFIC) {
            // Warning line with proposed inline NOLINT - highlight the proposed addition
            display_line = highlight_nolint_comments(line);
        } else {
            // Original source code line - no highlighting
            display_line = line;
        }
        
        std::string formatted_line = prefix + std::string(6 - line_num.length(), ' ') + line_num
                                     + " | " + display_line;
        context.context_lines.push_back(formatted_line);
    }

    context.warning_line_index = warning_index - start_line;
    context.format_preview = style_display_name(current_style);

    return context;
}

auto highlight_nolint_comments(const std::string& line) -> std::string {
    const std::string GREEN = "\033[32m";
    const std::string RESET = "\033[0m";

    std::string result = line;

    // List of NOLINT patterns to highlight
    const std::vector<std::string> nolint_patterns = {
        "// NOLINTNEXTLINE",
        "// NOLINTBEGIN", 
        "// NOLINTEND",
        "// NOLINT"  // Keep this last as it's a substring of the others
    };

    // Find and highlight any NOLINT pattern
    for (const auto& pattern : nolint_patterns) {
        auto nolint_pos = result.find(pattern);
        if (nolint_pos != std::string::npos) {
            size_t end_pos;
            
            // Check if there's a parenthesis for specific warnings
            auto paren_pos = result.find('(', nolint_pos);
            if (paren_pos != std::string::npos) {
                // Find the closing parenthesis
                auto close_paren_pos = result.find(')', paren_pos);
                if (close_paren_pos != std::string::npos) {
                    end_pos = close_paren_pos + 1; // Include the closing parenthesis
                } else {
                    // No closing parenthesis, highlight to end of line or whitespace
                    end_pos = result.find_first_of(" \t\n", paren_pos);
                    if (end_pos == std::string::npos) {
                        end_pos = result.length();
                    }
                }
            } else {
                // No parenthesis, highlight until whitespace or end of line
                end_pos = result.find_first_of(" \t\n", nolint_pos + pattern.length());
                if (end_pos == std::string::npos) {
                    end_pos = result.length();
                }
            }

            result.insert(end_pos, RESET);
            result.insert(nolint_pos, GREEN);
            break; // Only highlight the first match to avoid overlapping
        }
    }

    return result;
}

auto update_navigation(UIModel model, InputEvent event) -> UIModel {
    // FIXED: No longer needs total_warnings parameter - it's in the model
    size_t active_count = model.get_active_warning_count();

    switch (event) {
    case InputEvent::ARROW_LEFT:
        if (model.current_index > 0) {
            model.current_index--;
            model.show_boundary_message = false;
        } else {
            model.show_boundary_message = true;
            model.status_message = "Already at first warning.";
        }
        break;

    case InputEvent::ARROW_RIGHT:
        if (model.current_index < active_count - 1) {
            model.current_index++;
            model.show_boundary_message = false;
        } else {
            model.show_boundary_message = true;
            model.status_message = "Already at last warning.";
        }
        break;

    default:
        break;
    }

    return model;
}

// REMOVED: update_style_cycling function - logic moved to main update() function for cleaner architecture

auto update_search_mode(UIModel model, const std::string& search_input,
                        const std::vector<Warning>& warnings) -> UIModel {
    model.search_input = search_input;
    model.filtered_indices = filter_warnings(warnings, search_input);

    // Adjust current index if needed
    if (model.current_index >= model.filtered_indices.size()) {
        model.current_index
            = model.filtered_indices.empty() ? 0 : model.filtered_indices.size() - 1;
    }

    // Update status message
    if (search_input.empty()) {
        model.status_message
            = "Filter cleared - showing all " + std::to_string(warnings.size()) + " warnings";
    } else if (model.filtered_indices.empty()) {
        model.status_message = "No warnings match filter '" + search_input + "' - showing all "
                               + std::to_string(warnings.size()) + " warnings";
        model.filtered_indices.clear(); // Show all warnings when no matches
    } else {
        model.status_message = "Applied filter: '" + search_input + "' - showing "
                               + std::to_string(model.filtered_indices.size()) + "/"
                               + std::to_string(warnings.size()) + " warnings";
    }

    return model;
}

} // namespace nolint::functional_core