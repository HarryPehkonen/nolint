#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace nolint {

// Warning from clang-tidy
struct Warning {
    std::string file_path;
    int line_number;
    int column;
    std::string type;
    std::string message;
};

// Suppression style options
enum class NolintStyle {
    NONE,
    NOLINT,           // Inline comment: // NOLINT(type)
    NOLINTNEXTLINE,   // Previous line: // NOLINTNEXTLINE(type)
    NOLINT_BLOCK      // Block: // NOLINTBEGIN(type) ... // NOLINTEND(type)
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
    UNKNOWN
};

// Complete UI state (immutable model)
struct UIModel {
    // Core data
    std::vector<Warning> warnings;
    size_t current_index = 0;
    
    // User decisions
    std::unordered_map<size_t, NolintStyle> decisions;  // warning index -> style
    
    // File tracking
    std::unordered_set<std::string> modified_files;  // Files that will be changed
    
    // UI state
    bool should_exit = false;
    bool show_statistics = false;
    bool dry_run = false;  // Preview mode - don't actually save files
    std::string search_filter;
    
    // Helper methods
    auto total_warnings() const -> size_t { return warnings.size(); }
    auto has_warnings() const -> bool { return !warnings.empty(); }
    
    auto current_warning() const -> const Warning& { 
        return warnings[current_index]; 
    }
    
    auto get_decision(size_t index) const -> NolintStyle {
        auto it = decisions.find(index);
        return (it != decisions.end()) ? it->second : NolintStyle::NONE;
    }
    
    auto current_style() const -> NolintStyle {
        return get_decision(current_index);
    }
};

// Pure update function - the heart of Model-View-Update pattern
auto update(UIModel model, InputEvent event) -> UIModel;

} // namespace nolint