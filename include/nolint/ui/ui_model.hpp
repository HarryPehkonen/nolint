#pragma once

#include "nolint/core/warning.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nolint {

// Input events from terminal
enum class InputEvent {
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    SAVE_EXIT,
    QUIT,
    SEARCH,
    SHOW_STATISTICS,
    ESCAPE,
    ENTER,
    UNKNOWN
};

// View modes for the UI
enum class ViewMode {
    REVIEWING,
    SEARCHING,
    STATISTICS,
    EXIT
};

// Statistics for warning types
struct WarningTypeStats {
    std::string warning_type;
    size_t total_count{};
    size_t addressed_count{};
    size_t visited_count{};
    
    auto addressed_percentage() const -> int
    {
        return total_count > 0 ? static_cast<int>((addressed_count * 100) / total_count) : 0;
    }
};

// Decisions map type
using Decisions = std::unordered_map<std::string, NolintStyle>;

// Decision persistence functions
auto save_decisions(const Decisions& decisions, const std::string& file_path) -> bool;
auto load_decisions(const std::string& file_path) -> std::optional<Decisions>;

// Immutable UI state - ALL UI state in one place
struct UIModel {
    // --- Data State (ADDED: Warnings and decisions are part of UI state) ---
    std::vector<Warning> warnings;
    Decisions decisions;
    
    // Core navigation
    size_t current_index{};
    ViewMode mode = ViewMode::REVIEWING;
    // NOTE: current_style removed - always derived from decisions via get_current_style()
    
    // Search state (when mode == SEARCHING)
    std::string search_input;
    std::vector<size_t> filtered_indices;  // Computed, cached for performance
    
    // Statistics state (when mode == STATISTICS)  
    size_t current_stats_index{};
    std::vector<WarningTypeStats> warning_stats;
    
    // UX polish state (essential for good user experience)
    std::unordered_set<std::string> visited_warnings;  // Track progress
    bool show_boundary_message = false;                // "Already at first warning"
    std::string status_message;                        // User feedback
    bool quit_confirmation_needed = false;             // Prevent accidental quit
    
    // Helper: Get active warnings count (filtered or original)
    auto get_active_warning_count() const -> size_t
    {
        return filtered_indices.empty() ? warnings.size() : filtered_indices.size();
    }
    
    // Helper: Get current warning index (filtered or direct)
    auto get_actual_warning_index() const -> size_t
    {
        return filtered_indices.empty() ? current_index : filtered_indices[current_index];
    }
    
    // Helper: Get current warning style from decisions
    auto get_current_style() const -> NolintStyle
    {
        if (warnings.empty()) return NolintStyle::NONE;
        auto key = warning_key(warnings[get_actual_warning_index()]);
        auto it = decisions.find(key);
        return (it != decisions.end()) ? it->second : NolintStyle::NONE;
    }
};

// Screen structure for declarative rendering
struct Line {
    std::string text;
    bool is_highlighted = false;
};

struct Screen {
    std::vector<Line> content;
    std::string status_line;
    std::string control_hints;
};

} // namespace nolint