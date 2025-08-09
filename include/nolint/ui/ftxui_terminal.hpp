#pragma once

#include "nolint/interfaces.hpp"
#include "nolint/ui/ui_model.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <memory>
#include <functional>
#include <optional>

namespace nolint {

class FTXUITerminal : public ITerminal {
public:
    FTXUITerminal();
    ~FTXUITerminal() override;
    
    // Delete copy operations to ensure single instance
    FTXUITerminal(const FTXUITerminal&) = delete;
    auto operator=(const FTXUITerminal&) -> FTXUITerminal& = delete;
    
    // ITerminal interface
    auto setup_raw_mode() -> bool override;
    auto get_input_event() -> InputEvent override;
    auto display_screen(const Screen& screen) -> void override;
    auto read_line() -> std::string override;
    auto is_interactive() -> bool override;
    auto restore_terminal_state() -> void override;

    // FTXUI-specific reactive interface
    auto run_reactive_session(
        const UIModel& initial_model,
        std::function<UIModel(UIModel, InputEvent)> update_function
    ) -> UIModel;

private:
    ftxui::Component main_component_;
    
    // State for event handling
    bool raw_mode_active_ = false;
    InputEvent last_event_ = InputEvent::UNKNOWN;
    std::string line_input_buffer_;
    bool reading_line_ = false;
    
    // Current UI state
    const Screen* current_screen_ = nullptr;
    
    // Helper methods
    auto map_ftxui_event_to_input_event(const ftxui::Event& event) -> InputEvent;
    auto create_interactive_component(
        const UIModel& model,
        std::function<UIModel(UIModel, InputEvent)> update_function
    ) -> ftxui::Component;
    auto screen_to_ftxui_element(const Screen& screen) -> ftxui::Element;
    
    // Specialized screen composers
    auto compose_review_screen_ftxui(const UIModel& model) -> ftxui::Element;
    auto compose_statistics_screen_ftxui(const UIModel& model) -> ftxui::Element;
    auto compose_search_screen_ftxui(const UIModel& model) -> ftxui::Element;
    
    // File context helpers
    auto build_file_context(const Warning& warning, NolintStyle style) -> std::vector<std::string>;
    auto read_file_lines(const std::string& file_path) -> std::vector<std::string>;
};

} // namespace nolint