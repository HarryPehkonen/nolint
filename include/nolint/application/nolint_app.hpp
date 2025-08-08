#pragma once

#include "nolint/core/warning.hpp"
#include "nolint/interfaces.hpp"
#include "nolint/ui/ui_model.hpp"
#include <memory>
#include <string>
#include <vector>

namespace nolint {

struct Config {
    std::string input_file = "-";           // stdin by default
    bool interactive = true;
    NolintStyle default_style = NolintStyle::NOLINT_SPECIFIC;
    bool dry_run = false;
    std::string load_session_file;          // Load decisions from this file
    std::string save_session_file;          // Save decisions to this file
};

class NolintApp {
private:
    std::unique_ptr<ITerminal> terminal_;
    std::unique_ptr<IFileSystem> filesystem_;
    std::unique_ptr<IWarningParser> parser_;
    
public:
    NolintApp(std::unique_ptr<ITerminal> terminal,
              std::unique_ptr<IFileSystem> filesystem,
              std::unique_ptr<IWarningParser> parser);
    
    auto run(const Config& config) -> int;

private:
    // Functional reactive UI - Model-View-Update pattern (FIXED: Only UIModel)
    auto run_interactive(UIModel initial_model) -> Decisions;
    auto run_batch_mode(const UIModel& model, const Config& config) -> Decisions;
    auto update(UIModel model, InputEvent event) -> UIModel;
    auto render(const UIModel& model) -> Screen;
    
    // Screen composition functions (FIXED: Only UIModel)
    auto compose_review_screen(const UIModel& model) -> Screen;
    auto compose_statistics_screen(const UIModel& model) -> Screen;
    auto compose_search_screen(const UIModel& model) -> Screen;
    
    // Application logic (FIXED: load_warnings returns warnings instead of storing them)
    auto load_warnings(const Config& config) -> std::vector<Warning>;
    auto apply_decisions_to_files(const Decisions& decisions, const std::vector<Warning>& warnings) -> bool;
    auto show_summary(const Decisions& decisions) -> void;
    
    // Helper functions (FIXED: no longer needed with UIModel helpers)
    auto handle_quit_confirmation(UIModel& model) -> bool;
};

} // namespace nolint