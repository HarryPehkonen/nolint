#include "nolint/ui/ftxui_terminal.hpp"
#include "nolint/core/functional_core.hpp"
#include "nolint/io/file_system.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

namespace nolint {

FTXUITerminal::FTXUITerminal() {
    // Initialize screen when needed in run_reactive_session
}

FTXUITerminal::~FTXUITerminal() {
    restore_terminal_state();
}

auto FTXUITerminal::setup_raw_mode() -> bool {
    // FTXUI handles raw mode internally - no need to do anything
    raw_mode_active_ = true;
    return true;
}

auto FTXUITerminal::get_input_event() -> InputEvent {
    // Legacy method - not used in reactive mode
    // This is only here for backward compatibility with ITerminal interface
    return InputEvent::UNKNOWN;
}

auto FTXUITerminal::display_screen(const Screen& screen) -> void {
    // Legacy method for backward compatibility with ITerminal interface
    // Store the current screen for potential use in get_input_event
    current_screen_ = &screen;
    
    // In FTXUI reactive mode, rendering is handled by the component loop
    // For legacy compatibility, we can still render to stdout if needed
    auto element = screen_to_ftxui_element(screen);
    
    // Create a screen and render to stdout for immediate display
    auto ftxui_screen = ftxui::Screen::Create(
        ftxui::Dimension::Full(),
        ftxui::Dimension::Full()
    );
    
    ftxui::Render(ftxui_screen, element);
    std::cout << ftxui_screen.ToString() << std::flush;
}

auto FTXUITerminal::read_line() -> std::string {
    // Legacy method for search input - in FTXUI reactive mode,
    // search input is handled directly in the event loop
    std::string result;
    bool input_complete = false;
    
    auto input_component = ftxui::Input(&result, "Enter search terms...");
    
    auto container = ftxui::Container::Vertical({
        ftxui::Renderer([this] {
            if (current_screen_) {
                return screen_to_ftxui_element(*current_screen_);
            }
            return ftxui::text("Search Mode");
        }),
        input_component
    });
    
    // Add event handler to detect when Enter is pressed
    auto event_handler = ftxui::CatchEvent(container, 
        [&input_complete](ftxui::Event event) {
            if (event == ftxui::Event::Return) {
                input_complete = true;
                return true;
            }
            if (event == ftxui::Event::Escape) {
                input_complete = true;
                return true;
            }
            return false;
        }
    );
    
    // Run the input loop until Enter or Escape is pressed
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    while (!input_complete) {
        screen.Loop(event_handler);
    }
    
    return result;
}

auto FTXUITerminal::is_interactive() -> bool {
    // FTXUI handles this internally, assume interactive if we got this far
    return true;
}

auto FTXUITerminal::restore_terminal_state() -> void {
    if (raw_mode_active_) {
        // FTXUI handles terminal restoration in its destructor
        raw_mode_active_ = false;
    }
}

auto FTXUITerminal::run_reactive_session(
    const UIModel& initial_model,
    std::function<UIModel(UIModel, InputEvent)> update_function
) -> UIModel {
    using namespace ftxui;
    
    // Create a mutable copy of the model for the reactive loop
    auto current_model = initial_model;
    
    // Simple direct approach: create component and screen instance
    auto screen = ScreenInteractive::Fullscreen();
    
    // Create the component with direct event handling
    auto component = CatchEvent(
        Renderer([&] {
            switch (current_model.mode) {
            case ViewMode::REVIEWING:
                return compose_review_screen_ftxui(current_model);
            case ViewMode::STATISTICS:
                return compose_statistics_screen_ftxui(current_model);
            case ViewMode::SEARCHING:
                return compose_search_screen_ftxui(current_model);
            case ViewMode::EXIT:
                return text("Exiting...") | center;
            default:
                return text("Unknown mode") | center;
            }
        }),
        [&](Event event) -> bool {
            // Handle events and update model
            if (event == Event::Character('q') || event == Event::Character('Q')) {
                current_model = update_function(current_model, InputEvent::QUIT);
                if (current_model.mode == ViewMode::EXIT) {
                    screen.ExitLoopClosure()();
                }
                return true;
            }
            
            if (event == Event::Character('x') || event == Event::Character('X')) {
                current_model = update_function(current_model, InputEvent::SAVE_EXIT);
                if (current_model.mode == ViewMode::EXIT) {
                    screen.ExitLoopClosure()();
                }
                return true;
            }
            
            if (event == Event::ArrowUp) {
                current_model = update_function(current_model, InputEvent::ARROW_UP);
                return true;
            }
            
            if (event == Event::ArrowDown) {
                current_model = update_function(current_model, InputEvent::ARROW_DOWN);
                return true;
            }
            
            if (event == Event::ArrowLeft) {
                current_model = update_function(current_model, InputEvent::ARROW_LEFT);
                return true;
            }
            
            if (event == Event::ArrowRight) {
                current_model = update_function(current_model, InputEvent::ARROW_RIGHT);
                return true;
            }
            
            if (event == Event::Character('/')) {
                current_model = update_function(current_model, InputEvent::SEARCH);
                return true;
            }
            
            if (event == Event::Character('t') || event == Event::Character('T')) {
                current_model = update_function(current_model, InputEvent::SHOW_STATISTICS);
                return true;
            }
            
            if (event == Event::Escape) {
                current_model = update_function(current_model, InputEvent::ESCAPE);
                return true;
            }
            
            if (event == Event::Return) {
                current_model = update_function(current_model, InputEvent::ENTER);
                return true;
            }
            
            return false; // Event not handled
        }
    );
    
    // Run the loop
    screen.Loop(component);
    return current_model;
}

auto FTXUITerminal::map_ftxui_event_to_input_event(const ftxui::Event& event) -> InputEvent {
    // Map FTXUI events to our InputEvent enum
    if (event == ftxui::Event::ArrowUp) {
        return InputEvent::ARROW_UP;
    }
    if (event == ftxui::Event::ArrowDown) {
        return InputEvent::ARROW_DOWN;
    }
    if (event == ftxui::Event::ArrowLeft) {
        return InputEvent::ARROW_LEFT;
    }
    if (event == ftxui::Event::ArrowRight) {
        return InputEvent::ARROW_RIGHT;
    }
    if (event == ftxui::Event::Escape) {
        return InputEvent::ESCAPE;
    }
    if (event == ftxui::Event::Return) {
        return InputEvent::ENTER;
    }
    
    // Character input - only map single character commands
    if (event.is_character()) {
        std::string chars = event.character();
        if (!chars.empty() && chars.length() == 1) {
            char ch = chars[0];
            switch (ch) {
                case 'x':
                case 'X':
                    return InputEvent::SAVE_EXIT;
                case 'q':
                case 'Q':
                    return InputEvent::QUIT;
                case '/':
                    return InputEvent::SEARCH;
                case 't':
                case 'T':
                    return InputEvent::SHOW_STATISTICS;
                default:
                    // Don't map other characters to UNKNOWN during search mode
                    // Let the event handler deal with text input
                    return InputEvent::UNKNOWN;
            }
        }
    }
    
    return InputEvent::UNKNOWN;
}

auto FTXUITerminal::screen_to_ftxui_element(const Screen& screen) -> ftxui::Element {
    using namespace ftxui;
    
    // Convert legacy Screen struct to FTXUI elements
    // This is used for backward compatibility with the existing ITerminal interface
    
    // Convert screen content to FTXUI elements
    Elements content_elements;
    
    for (const auto& line : screen.content) {
        if (line.is_highlighted) {
            content_elements.push_back(text(line.text) | color(Color::Green));
        } else {
            content_elements.push_back(text(line.text));
        }
    }
    
    // Build the complete UI
    auto content_box = vbox(std::move(content_elements));
    
    auto status_element = text(screen.status_line) | bold | color(Color::Cyan);
    auto hints_element = text(screen.control_hints) | dim;
    
    // Combine all elements with separators
    return vbox({
        content_box | flex,
        separator(),
        status_element,
        hints_element
    });
}

auto FTXUITerminal::create_interactive_component(
    const UIModel& model,
    std::function<UIModel(UIModel, InputEvent)> update_function
) -> ftxui::Component {
    using namespace ftxui;
    
    // This method is now primarily used by run_interactive_session
    // Create a component that can handle events and update state
    return CatchEvent(
        Renderer([this, &model] {
            switch (model.mode) {
            case ViewMode::REVIEWING:
                return compose_review_screen_ftxui(model);
            case ViewMode::STATISTICS:
                return compose_statistics_screen_ftxui(model);
            case ViewMode::SEARCHING:
                return compose_search_screen_ftxui(model);
            case ViewMode::EXIT:
                return text("Exiting...") | center;
            default:
                return text("Unknown mode") | center;
            }
        }),
        [this, &update_function](Event event) {
            // Basic event handler - full implementation in run_interactive_session
            auto input_event = map_ftxui_event_to_input_event(event);
            return input_event != InputEvent::UNKNOWN;
        }
    );
}

auto FTXUITerminal::compose_review_screen_ftxui(const UIModel& model) -> ftxui::Element {
    using namespace ftxui;
    
    if (model.warnings.empty()) {
        return vbox({
            text("No warnings to review.") | center,
            separator(),
            text("No warnings found") | bold | color(Color::Cyan),
            text("Press 'q' to quit") | dim
        });
    }

    size_t active_count = model.get_active_warning_count();
    if (model.current_index >= active_count) {
        return vbox({
            text("Invalid warning index.") | center,
            separator(),
            text("Error") | bold | color(Color::Red),
            text("Press 'q' to quit") | dim
        });
    }

    size_t actual_index = model.get_actual_warning_index();
    if (actual_index >= model.warnings.size()) {
        return vbox({
            text("Invalid warning index.") | center,
            separator(),
            text("Error") | bold | color(Color::Red),
            text("Press 'q' to quit") | dim
        });
    }

    const auto& warning = model.warnings[actual_index];

    // Header
    auto header = text("=== Interactive NOLINT Tool ===") | bold | center;
    
    // Warning info box
    Elements warning_info = {
        text("┌─ Warning " + std::to_string(model.current_index + 1)
             + "/" + std::to_string(active_count) + " ─"),
        text("│ File: " + warning.file_path),
        text("│ Line: " + std::to_string(warning.line_number) + ":"
             + std::to_string(warning.column_number)),
        text("│ Type: " + warning.warning_type),
        text("│ Message: " + warning.message),
        text("│")
    };
    
    // File context with proper display for NOLINT_BLOCK
    warning_info.push_back(text("│ Context:"));
    
    auto file_context = build_file_context(warning, model.get_current_style());
    for (const auto& context_line : file_context) {
        warning_info.push_back(text("│ " + context_line));
    }
    
    warning_info.push_back(text("│"));
    
    // Current style
    auto current_style_name = [&model]() {
        switch (model.get_current_style()) {
            case NolintStyle::NONE: return "None (no suppression)";
            case NolintStyle::NOLINT_SPECIFIC: return "NOLINT(check-name)";
            case NolintStyle::NOLINTNEXTLINE: return "NOLINTNEXTLINE(check-name)";
            case NolintStyle::NOLINT_BLOCK: return "NOLINT_BLOCK";
            default: return "Unknown";
        }
    }();
    
    warning_info.push_back(text("│ Apply NOLINT? Format: " + std::string(current_style_name)));
    warning_info.push_back(text("└─"));
    
    auto warning_box = vbox(std::move(warning_info)) | border;
    
    // Status line
    size_t suppression_count = std::count_if(
        model.decisions.begin(), model.decisions.end(),
        [](const auto& pair) { return pair.second != NolintStyle::NONE; }
    );
    
    std::string status_text;
    if (model.quit_confirmation_needed) {
        status_text = model.status_message;
    } else if (model.show_boundary_message) {
        status_text = model.status_message;
    } else if (!model.filtered_indices.empty()) {
        status_text = "Showing " + std::to_string(model.filtered_indices.size()) + "/"
                     + std::to_string(model.warnings.size()) + " warnings (filtered: '"
                     + model.search_input + "')";
    } else {
        status_text = "Suppressions: " + std::to_string(suppression_count) + " | Warning "
                     + std::to_string(model.current_index + 1) + "/"
                     + std::to_string(active_count);
    }
    
    auto status = text(status_text) | bold | color(Color::Cyan);
    auto hints = text("Navigate [←→] Style [↑↓] Save & Exit [x] Quit [q] Search [/] Stats [t]") | dim;
    
    return vbox({
        header,
        text(""),
        warning_box | flex,
        separator(),
        status,
        hints
    });
}

auto FTXUITerminal::compose_statistics_screen_ftxui(const UIModel& model) -> ftxui::Element {
    using namespace ftxui;
    
    auto header = text("=== Warning Type Summary ===") | bold | center;
    
    // Summary statistics
    size_t total_warnings = model.warnings.size();
    size_t addressed_count = std::count_if(
        model.decisions.begin(), model.decisions.end(),
        [](const auto& pair) { return pair.second != NolintStyle::NONE; }
    );
    size_t visited_count = model.visited_warnings.size();
    
    auto summary = text(
        "Total: " + std::to_string(total_warnings)
        + " warnings | Addressed: " + std::to_string(addressed_count) + " ("
        + std::to_string(addressed_count * 100 / std::max(total_warnings, size_t(1)))
        + "%)" + " | Visited: " + std::to_string(visited_count)
    );
    
    // Table header
    Elements table_content = {
        text("┌─────────────────────────────────────┬─────────┬─────────────┬─────────┐"),
        text("│ Warning Type                        │  Total  │  Addressed  │ Visited │"),
        text("├─────────────────────────────────────┼─────────┼─────────────┼─────────┤")
    };
    
    // Table rows
    for (size_t i = 0; i < model.warning_stats.size(); ++i) {
        const auto& stats = model.warning_stats[i];
        
        std::string selection_marker = (i == model.current_stats_index) ? ">> " : "   ";
        std::string line = "│ " + selection_marker + stats.warning_type;
        
        // Pad to fit column width
        if (line.length() < 40) {
            line += std::string(40 - line.length(), ' ');
        }
        
        line += "│   " + std::to_string(stats.total_count) + "   │    "
               + std::to_string(stats.addressed_count) + " ("
               + std::to_string(stats.addressed_percentage()) + "%)  │    "
               + std::to_string(stats.visited_count) + "    │";
        
        auto row_element = text(line);
        if (i == model.current_stats_index) {
            row_element |= color(Color::Yellow) | bold;  // Highlight selected row
        }
        table_content.push_back(row_element);
    }
    
    table_content.push_back(
        text("└─────────────────────────────────────┴─────────┴─────────────┴─────────┘")
    );
    
    auto table = vbox(std::move(table_content)) | border;
    
    auto status = text("Statistics Mode") | bold | color(Color::Cyan);
    auto hints = text("Navigate [↑↓] Filter [Enter] Back [Escape]") | dim;
    
    return vbox({
        header,
        text(""),
        summary,
        text(""),
        table | flex,
        separator(),
        status,
        hints
    });
}

auto FTXUITerminal::compose_search_screen_ftxui(const UIModel& model) -> ftxui::Element {
    using namespace ftxui;
    
    auto header = text("=== Search / Filter Warnings ===") | bold | center;
    
    Elements content = {
        text(""),
        text("Enter search terms (space-separated for AND logic):"),
        text("Searches across: file path, warning type, message, line numbers"),
        text(""),
        text("Current filter: '" + model.search_input + "'") | color(Color::Yellow)
    };
    
    auto search_box = vbox(std::move(content)) | border;
    
    auto status = text("Search Mode - Enter search terms, then press Enter") | bold | color(Color::Cyan);
    auto hints = text("Type search terms, [Enter] to apply, [Escape] to cancel") | dim;
    
    return vbox({
        header,
        text(""),
        search_box | flex,
        separator(),
        status,
        hints
    });
}

auto FTXUITerminal::read_file_lines(const std::string& file_path) -> std::vector<std::string> {
    std::vector<std::string> lines;
    std::ifstream file(file_path);
    
    if (!file.is_open()) {
        return lines; // Return empty vector if file can't be opened
    }
    
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

auto FTXUITerminal::build_file_context(const Warning& warning, NolintStyle style) -> std::vector<std::string> {
    auto file_lines = read_file_lines(warning.file_path);
    
    if (file_lines.empty() || warning.line_number == 0 || warning.line_number > file_lines.size()) {
        return {"(Could not read file context)"};
    }
    
    std::vector<std::string> context;
    
    if (style == NolintStyle::NOLINT_BLOCK) {
        // For NOLINT_BLOCK, show context around both BEGIN and END locations
        // Use half the normal context size for each location
        constexpr size_t full_context_size = 5;
        constexpr size_t half_context_size = full_context_size / 2; // = 2
        
        size_t warning_index = warning.line_number - 1;
        
        // Calculate function bounds for NOLINT_BLOCK
        size_t function_start = warning_index;
        size_t function_end = warning_index;
        
        if (warning.function_lines.has_value()) {
            function_end = warning_index + warning.function_lines.value() - 1;
            function_end = std::min(function_end, file_lines.size() - 1);
        }
        
        // Context around NOLINTBEGIN (start of function)
        context.push_back("NOLINTBEGIN location:");
        size_t begin_start = function_start >= half_context_size ? function_start - half_context_size : 0;
        size_t begin_end = std::min(function_start + half_context_size + 1, file_lines.size());
        
        for (size_t i = begin_start; i < begin_end; ++i) {
            std::string prefix = (i == function_start) ? ">>> " : "    ";
            context.push_back(prefix + std::to_string(i + 1) + ": " + file_lines[i]);
        }
        
        context.push_back("");
        context.push_back("NOLINTEND location:");
        
        // Context around NOLINTEND (end of function)
        size_t end_start = function_end >= half_context_size ? function_end - half_context_size : 0;
        size_t end_end = std::min(function_end + half_context_size + 1, file_lines.size());
        
        for (size_t i = end_start; i < end_end; ++i) {
            std::string prefix = (i == function_end) ? ">>> " : "    ";
            context.push_back(prefix + std::to_string(i + 1) + ": " + file_lines[i]);
        }
        
    } else {
        // For other styles, show normal context around the warning line
        constexpr size_t context_size = 5;
        size_t warning_index = warning.line_number - 1;
        size_t start_line = warning_index >= context_size ? warning_index - context_size : 0;
        size_t end_line = std::min(warning_index + context_size + 1, file_lines.size());
        
        for (size_t i = start_line; i < end_line; ++i) {
            std::string prefix = (i == warning_index) ? ">>> " : "    ";
            
            // Apply highlighting for NOLINT comments if this is a preview
            std::string line_content = file_lines[i];
            if (style != NolintStyle::NONE && i == warning_index) {
                // Show preview of where NOLINT comment would be added
                switch (style) {
                case NolintStyle::NOLINT_SPECIFIC:
                    line_content += "  // NOLINT(" + warning.warning_type + ")"; 
                    break;
                case NolintStyle::NOLINTNEXTLINE:
                    // NOLINTNEXTLINE goes on the line before
                    if (i > 0 && i == warning_index) {
                        context.push_back(">>> " + std::to_string(i) + ": " + file_lines[i-1] + "  // NOLINTNEXTLINE(" + warning.warning_type + ")");
                    }
                    break;
                default:
                    break;
                }
            }
            
            context.push_back(prefix + std::to_string(i + 1) + ": " + line_content);
        }
    }
    
    return context;
}

} // namespace nolint