#pragma once

#include "nolint/core/annotated_file.hpp"
#include "nolint/core/warning.hpp"
#include "nolint/ui/ui_model.hpp"
#include <ranges>  // For C++20 ranges optimization
#include <span>
#include <string_view>
#include <vector>

namespace nolint::functional_core {

// Text transformation functions (pure)
auto apply_decision(AnnotatedFile& file, const Warning& warning, NolintStyle style) -> void;

// OPTIMIZED: Accept any range of strings (not just vector) - template definition in header
template<std::ranges::range Range>
auto find_function_boundaries(Range&& line_range, const Warning& warning) -> std::pair<size_t, size_t> {
    if (!warning.function_lines.has_value() || warning.line_number == 0) {
        return {warning.line_number - 1, warning.line_number - 1};
    }

    // Simple function boundary detection
    // Start from warning line, look backwards for function signature  
    size_t start_line = warning.line_number - 1;
    size_t end_line = start_line + warning.function_lines.value() - 1;

    // Get range size using C++20 ranges
    auto range_size = std::ranges::size(line_range);
    
    // Clamp to valid range
    start_line = std::min(start_line, range_size > 0 ? range_size - 1 : 0);
    end_line = std::min(end_line, range_size > 0 ? range_size - 1 : 0);

    return {start_line, end_line};
}

// Warning filtering and search (pure)
auto filter_warnings(std::span<const Warning> warnings, std::string_view filter_terms)
    -> std::vector<size_t>;

auto split_by_whitespace(std::string_view text) -> std::vector<std::string>;
auto to_lowercase(std::string_view text) -> std::string;
auto trim(std::string_view text) -> std::string;

// Statistics calculation (pure)
auto calculate_statistics(const std::vector<Warning>& warnings, const Decisions& decisions,
                         const std::unordered_set<std::string>& visited_warnings)
    -> std::vector<WarningTypeStats>;

// Context building for display (pure)
struct DisplayContext {
    std::vector<std::string> context_lines;
    size_t warning_line_index{};
    std::string format_preview;
};

auto build_display_context(const Warning& warning, const std::vector<std::string>& file_lines,
                          NolintStyle current_style) -> DisplayContext;

// Highlight NOLINT comments in text (pure)
auto highlight_nolint_comments(const std::string& line) -> std::string;

// UI update functions (pure state transitions)
auto update_navigation(UIModel model, InputEvent event) -> UIModel;
auto update_search_mode(UIModel model, const std::string& search_input,
                       const std::vector<Warning>& warnings) -> UIModel;
// NOTE: update_style_cycling removed - now handled directly in main update() function

} // namespace nolint::functional_core