#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace nolint {

// Warning from clang-tidy
struct Warning {
    std::string file_path;
    int line_number;
    int column;
    std::string type;
    std::string message;
    std::optional<int> function_lines;  // For readability-function-size warnings
};

// Suppression style options
enum class NolintStyle {
    NONE,
    NOLINT,           // Inline comment: // NOLINT(type)
    NOLINTNEXTLINE,   // Previous line: // NOLINTNEXTLINE(type)
    NOLINT_BLOCK      // Block: // NOLINTBEGIN(type) ... // NOLINTEND(type)
};

// Statistics for a warning type
struct WarningTypeStats {
    std::string type;
    int total_count = 0;
    int nolint_count = 0;
    int nolintnextline_count = 0; 
    int nolint_block_count = 0;
    int unsuppressed_count = 0;
};

// Input events we handle
enum class InputEvent {
    QUIT,
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    SEARCH,
    SHOW_STATISTICS,
    SAVE_EXIT,
    ENTER,
    ESCAPE,
    UNKNOWN
};

// Complete UI state (immutable model)
struct UIModel {
    // Core data
    std::vector<Warning> warnings;
    std::vector<size_t> filtered_warning_indices;  // Indices of warnings that match current filter
    size_t current_index = 0;  // Index in filtered_warning_indices, not warnings
    
    // User decisions
    std::unordered_map<size_t, NolintStyle> decisions;  // warning index -> style
    
    // File tracking
    std::unordered_set<std::string> modified_files;  // Files that will be changed
    
    // UI state
    bool should_exit = false;
    bool should_save = true;
    bool show_statistics = false;
    bool dry_run = false;  // Preview mode - don't actually save files
    bool in_search_mode = false;  // True when user is entering search filter
    std::string search_filter;
    
    // Statistics page state
    size_t statistics_selected_index = 0;  // Which warning type is selected in statistics view
    std::vector<std::string> statistics_types;  // Ordered list of warning types for selection
    
    // Helper methods
    auto total_warnings() const -> size_t { 
        return filtered_warning_indices.size(); 
    }
    auto has_warnings() const -> bool { return !warnings.empty(); }
    
    auto current_warning() const -> const Warning& {
        return warnings[filtered_warning_indices[current_index]];
    }
    
    auto current_warning_original_index() const -> size_t {
        return filtered_warning_indices[current_index];
    }
    
    auto get_decision(size_t original_warning_index) const -> NolintStyle {
        auto it = decisions.find(original_warning_index);
        return (it != decisions.end()) ? it->second : NolintStyle::NONE;
    }
    
    auto current_style() const -> NolintStyle {
        return get_decision(current_warning_original_index());
    }
};

// Filter warnings based on search string
auto filter_warnings(const std::vector<Warning>& warnings, const std::string& filter) -> std::vector<size_t>;

// Calculate statistics for all warning types with NOLINT status
auto calculate_warning_statistics(const std::vector<Warning>& warnings, 
                                const std::unordered_map<size_t, NolintStyle>& decisions) -> std::vector<WarningTypeStats>;

// Pure update function - the heart of Model-View-Update pattern
auto update(UIModel model, InputEvent event) -> UIModel;

} // namespace nolint
