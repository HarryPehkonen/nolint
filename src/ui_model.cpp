#include "ui_model.hpp"
#include <algorithm>
#include <cctype>

namespace nolint {

// Filter warnings based on search string - searches all fields
auto filter_warnings(const std::vector<Warning>& warnings, const std::string& filter)
    -> std::vector<size_t> {
    std::vector<size_t> filtered_indices;

    if (filter.empty()) {
        // No filter - return all indices
        for (size_t i = 0; i < warnings.size(); ++i) {
            filtered_indices.push_back(i);
        }
        return filtered_indices;
    }

    // Convert filter to lowercase for case-insensitive search
    std::string lower_filter = filter;
    std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);

    for (size_t i = 0; i < warnings.size(); ++i) {
        const auto& warning = warnings[i];

        // Search in all warning fields (case-insensitive)
        std::string searchable_text
            = warning.file_path + " " + warning.type + " " + warning.message;
        std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(),
                       ::tolower);

        if (searchable_text.find(lower_filter) != std::string::npos) {
            filtered_indices.push_back(i);
        }
    }

    return filtered_indices;
}

// Pure state transition function - no side effects!
auto calculate_warning_statistics(const std::vector<Warning>& warnings,
                                  const std::unordered_map<size_t, NolintStyle>& decisions)
    -> std::vector<WarningTypeStats> {
    std::unordered_map<std::string, WarningTypeStats> stats_map;

    // Initialize stats for each warning type
    for (size_t i = 0; i < warnings.size(); ++i) {
        const auto& warning = warnings[i];
        if (stats_map.find(warning.type) == stats_map.end()) {
            stats_map[warning.type] = WarningTypeStats{.type = warning.type};
        }

        auto& stats = stats_map[warning.type];
        stats.total_count++;

        // Count based on decision
        auto decision_it = decisions.find(i);
        NolintStyle style
            = (decision_it != decisions.end()) ? decision_it->second : NolintStyle::NONE;

        switch (style) {
        case NolintStyle::NOLINT:
            stats.nolint_count++;
            break;
        case NolintStyle::NOLINTNEXTLINE:
            stats.nolintnextline_count++;
            break;
        case NolintStyle::NOLINT_BLOCK:
            stats.nolint_block_count++;
            break;
        case NolintStyle::NONE:
            stats.unsuppressed_count++;
            break;
        }
    }

    // Convert map to sorted vector
    std::vector<WarningTypeStats> result;
    for (const auto& [type, stats] : stats_map) {
        result.push_back(stats);
    }

    // Sort by total count (descending)
    std::sort(result.begin(), result.end(),
              [](const WarningTypeStats& a, const WarningTypeStats& b) {
                  return a.total_count > b.total_count;
              });

    return result;
}

auto update(UIModel model, InputEvent event) -> UIModel {
    // Early return if no warnings
    if (model.warnings.empty()) {
        if (event == InputEvent::QUIT) {
            model.should_save = false;
            model.should_exit = true;
        }
        return model;
    }

    switch (event) {
    case InputEvent::QUIT:
        model.should_save = false;
        model.should_exit = true;
        break;

    case InputEvent::ARROW_LEFT:
        // Navigate to previous warning
        if (model.current_index > 0) {
            model.current_index--;
        }
        break;

    case InputEvent::ARROW_RIGHT:
        // Navigate to next warning
        if (model.current_index < model.total_warnings() - 1) {
            model.current_index++;
        }
        break;

    case InputEvent::ARROW_UP:
        if (model.show_statistics) {
            // Navigate statistics selection up
            if (model.statistics_selected_index > 0) {
                model.statistics_selected_index--;
            }
        } else {
            // Cycle suppression style forward
            auto current = static_cast<int>(model.current_style());
            const auto& warning = model.current_warning();

            // Skip NOLINT_BLOCK if warning doesn't have function_lines
            do {
                current = (current + 1) % 4;
            } while (current == static_cast<int>(NolintStyle::NOLINT_BLOCK)
                     && !warning.function_lines.has_value());

            model.decisions[model.current_warning_original_index()]
                = static_cast<NolintStyle>(current);

            // Track that this file will be modified
            if (model.current_style() != NolintStyle::NONE) {
                model.modified_files.insert(model.current_warning().file_path);
            }
        }
        break;

    case InputEvent::ARROW_DOWN:
        if (model.show_statistics) {
            // Navigate statistics selection down
            if (model.statistics_selected_index < model.statistics_types.size() - 1) {
                model.statistics_selected_index++;
            }
        } else {
            // Cycle suppression style backward
            auto current = static_cast<int>(model.current_style());
            const auto& warning = model.current_warning();

            // Skip NOLINT_BLOCK if warning doesn't have function_lines
            do {
                current = (current + 3) % 4; // +3 is same as -1 mod 4
            } while (current == static_cast<int>(NolintStyle::NOLINT_BLOCK)
                     && !warning.function_lines.has_value());

            model.decisions[model.current_warning_original_index()]
                = static_cast<NolintStyle>(current);

            // Track that this file will be modified
            if (model.current_style() != NolintStyle::NONE) {
                model.modified_files.insert(model.current_warning().file_path);
            }
        }
        break;

    case InputEvent::SHOW_STATISTICS:
        model.show_statistics = !model.show_statistics;
        if (model.show_statistics) {
            // Initialize statistics types and reset selection
            auto stats = calculate_warning_statistics(model.warnings, model.decisions);
            model.statistics_types.clear();
            for (const auto& stat : stats) {
                model.statistics_types.push_back(stat.type);
            }
            model.statistics_selected_index = 0;
        }
        break;

    case InputEvent::SAVE_EXIT:
        model.should_save = true;
        model.should_exit = true;
        break;

    case InputEvent::SEARCH:
        // Toggle search mode
        model.in_search_mode = !model.in_search_mode;
        break;

    case InputEvent::ENTER:
        if (model.show_statistics && !model.statistics_types.empty()) {
            // Select the highlighted warning type as filter
            std::string selected_type = model.statistics_types[model.statistics_selected_index];
            model.search_filter = selected_type;
            model.filtered_warning_indices = filter_warnings(model.warnings, model.search_filter);
            model.current_index = 0;       // Reset to first filtered result
            model.show_statistics = false; // Return to main view
        }
        break;

    case InputEvent::ESCAPE:
        if (model.show_statistics) {
            // Exit statistics mode without selecting
            model.show_statistics = false;
        }
        break;

    case InputEvent::UNKNOWN:
    default:
        // No state change
        break;
    }

    return model;
}

} // namespace nolint
