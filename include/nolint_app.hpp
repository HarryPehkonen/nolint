#pragma once

#include "file_io.hpp"
#include "functional_core.hpp"
#include "nolint_types.hpp"
#include "terminal_io.hpp"
#include "warning_parser.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nolint {

struct AppConfig {
    std::optional<std::string> input_file;
    bool read_stdin = false;
    NolintStyle default_style = NolintStyle::NOLINT_SPECIFIC;
    bool interactive = true;
    bool dry_run = false;
    bool force = false;
};

enum class UserAction {
    PREVIOUS,  // '←' - go to previous warning
    NEXT,      // '→' - go to next warning
    SAVE_EXIT, // 'x' - save and exit with summary
    QUIT,      // 'q' - quit without saving (with confirmation)
    ARROW_KEY, // ↑/↓ - cycle styles (style change handled separately)
    SEARCH     // '/' - enter search mode
};

// Consolidate all session state into a single struct
struct SessionState {
    // File management
    std::unordered_map<std::string, std::vector<std::string>> file_cache;
    std::vector<std::pair<Warning, NolintStyle>> decisions;

    // Warning navigation and choice memory
    std::unordered_map<std::string, NolintStyle> warning_decisions;

    // Search/filter state
    std::string current_filter;
    std::vector<Warning> filtered_warnings;
    std::vector<Warning> original_warnings;

    // Clear all state
    auto reset() -> void {
        file_cache.clear();
        decisions.clear();
        warning_decisions.clear();
        current_filter.clear();
        filtered_warnings.clear();
        original_warnings.clear();
    }
};

class NolintApp {
private:
    std::unique_ptr<IWarningParser> parser_;
    std::unique_ptr<IFileSystem> filesystem_;
    std::unique_ptr<ITerminal> terminal_;
    AppConfig config_;

    // All session state consolidated
    SessionState session_;

public:
    NolintApp(std::unique_ptr<IWarningParser> parser, std::unique_ptr<IFileSystem> filesystem,
              std::unique_ptr<ITerminal> terminal)
        : parser_(std::move(parser)), filesystem_(std::move(filesystem)),
          terminal_(std::move(terminal)) {}

    auto run(const AppConfig& config) -> int;

    // Search/filter functionality (public for testing)
    auto handle_search_input() -> void;
    auto apply_filter(const std::string& filter) -> void;
    auto get_active_warnings() const -> const std::vector<Warning>&;

private:
    auto parse_warnings() -> std::vector<Warning>;
    auto process_warnings(const std::vector<Warning>& warnings) -> void;
    auto display_warning(const Warning& warning, size_t index, size_t total,
                         NolintStyle current_style) -> void;
    auto get_user_decision_with_arrows(const Warning& warning, NolintStyle& current_style)
        -> UserAction;
    auto parse_input_char(char c, const Warning& warning, NolintStyle& current_style) -> UserAction;
    auto apply_decisions() -> bool;
    auto load_file(const std::string& path) -> std::vector<std::string>;
    auto save_file(const std::string& path, std::span<const std::string> lines) -> bool;
    auto build_context_display(const Warning& warning, std::span<const std::string> file_lines)
        -> std::string;
    auto build_context_with_preview(const DisplayContext& context, const Warning& warning)
        -> std::string;
    auto get_style_name(NolintStyle style) -> std::string;
    auto cycle_style(NolintStyle current, const Warning& warning, bool up) -> NolintStyle;
    auto get_warning_key(const Warning& warning) -> std::string;
    auto count_suppressions() const -> int;
};

} // namespace nolint