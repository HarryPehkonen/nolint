// Final version with automatic piped input detection and /dev/tty redirect
#include "ui_model.hpp"
#include "file_context.hpp"
#include "warning_parser.hpp"
#include "file_modifier.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <sys/stat.h>

struct Config {
    std::string input_file;
    bool use_stdin = true;
    bool dry_run = false;
    bool interactive = true;
};

auto parse_args(int argc, char* argv[]) -> Config {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            config.input_file = argv[++i];
            config.use_stdin = false;
        } else if (arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "--non-interactive") {
            config.interactive = false;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: nolint [options]\n";
            std::cout << "  -i, --input <file>     Read warnings from file\n";
            std::cout << "      --dry-run          Preview changes without modifying files\n";
            std::cout << "      --non-interactive  Apply default NOLINT style to all warnings\n";
            std::cout << "  -h, --help             Show this help\n";
            std::cout << "\nExamples:\n";
            std::cout << "  clang-tidy src/*.cpp | nolint                    # Automatic piped input handling\n";
            std::cout << "  nolint -i warnings.txt                          # File input\n";
            std::cout << "  clang-tidy src/*.cpp | nolint --dry-run          # Preview only\n";
            std::cout << "  clang-tidy src/*.cpp | nolint --non-interactive  # Batch mode\n";
            std::exit(0);
        }
    }
    
    return config;
}

// Advanced input type detection
enum class InputType {
    TERMINAL,     // Normal terminal input
    PIPE,         // Piped input (clang-tidy | nolint)
    FILE_REDIRECT,// File redirect (nolint < warnings.txt)
    UNKNOWN
};

auto detect_input_type() -> InputType {
    if (isatty(fileno(stdin))) {
        return InputType::TERMINAL;
    }
    
    // Use fstat to get more detailed information
    struct stat stdin_stat;
    if (fstat(fileno(stdin), &stdin_stat) == 0) {
        if (S_ISFIFO(stdin_stat.st_mode)) {
            return InputType::PIPE;
        } else if (S_ISREG(stdin_stat.st_mode)) {
            return InputType::FILE_REDIRECT;
        }
    }
    
    return InputType::UNKNOWN;
}

auto describe_input_type(InputType type) -> std::string {
    switch (type) {
        case InputType::TERMINAL: return "terminal";
        case InputType::PIPE: return "pipe";
        case InputType::FILE_REDIRECT: return "file redirect";
        case InputType::UNKNOWN: return "unknown";
    }
    return "unknown";
}

// UI mode selector
enum UIMode {
    MAIN_UI = 0,
    SEARCH_UI = 1
};

// Helper function to create balanced NOLINT_BLOCK preview
struct BalancedContext {
    std::vector<std::string> lines;
    std::string error_message;
};

auto create_balanced_nolint_block_preview(const nolint::Warning& warning, int function_lines, int context_lines = 2) -> BalancedContext {
    BalancedContext result;
    
    std::ifstream file(warning.file_path);
    if (!file.is_open()) {
        result.error_message = "Could not open file: " + warning.file_path;
        return result;
    }
    
    std::vector<std::string> all_lines;
    std::string line;
    while (std::getline(file, line)) {
        all_lines.push_back(line);
    }
    
    if (warning.line_number < 1 || warning.line_number > static_cast<int>(all_lines.size())) {
        result.error_message = "Line number out of range";
        return result;
    }
    
    // Extract indentation from the warning line
    std::string indent;
    int warning_idx = warning.line_number - 1;
    if (warning_idx >= 0 && warning_idx < static_cast<int>(all_lines.size())) {
        for (char c : all_lines[warning_idx]) {
            if (c == ' ' || c == '\t') {
                indent += c;
            } else {
                break;
            }
        }
    }
    
    // Show 6 lines before NOLINTBEGIN  
    int pre_begin_lines = std::min(context_lines * 3, 6); // Scale with context, max 6
    int start_line = std::max(0, warning.line_number - pre_begin_lines - 1);
    for (int i = start_line; i < warning.line_number - 1; ++i) {
        if (i >= 0 && i < static_cast<int>(all_lines.size())) {
            result.lines.push_back(std::to_string(i + 1) + ": " + all_lines[i]);
        }
    }
    
    // Add NOLINTBEGIN
    result.lines.push_back(std::string(std::to_string(warning.line_number).length() + 2, ' ') + indent + "// NOLINTBEGIN(" + warning.type + ")");
    
    // Show 6 lines of function start
    int post_begin_lines = std::min(context_lines * 3, 6); // Scale with context, max 6
    for (int i = warning.line_number - 1; i < std::min(warning.line_number - 1 + post_begin_lines, static_cast<int>(all_lines.size())); ++i) {
        if (i >= 0 && i < static_cast<int>(all_lines.size())) {
            result.lines.push_back(std::to_string(i + 1) + ": " + all_lines[i]);
        }
    }
    
    // Add continuation
    int displayed_function_lines = post_begin_lines;
    int pre_end_lines = std::min(context_lines * 3, 6); // Scale with context, max 6
    int remaining_lines = function_lines - displayed_function_lines - pre_end_lines;
    if (remaining_lines > 0) {
        result.lines.push_back(std::string(12, ' ') + "... (" + std::to_string(remaining_lines) + " more lines)");
    }
    
    // Find the actual function's closing brace position after function_end
    int function_end = warning.line_number + function_lines - 1;
    int closing_brace_line = function_end;
    
    // Look for the function's closing brace (should be at the function indentation level)
    // Use the already extracted function indent from earlier in the function
    
    for (int i = function_end; i < std::min(function_end + 5, static_cast<int>(all_lines.size())); ++i) {
        if (i >= 0 && all_lines[i].find('}') != std::string::npos) {
            // Check if this closing brace is at the function level (same indentation as function)
            std::string line_indent;
            for (char c : all_lines[i]) {
                if (c == ' ' || c == '\t') {
                    line_indent += c;
                } else {
                    break;
                }
            }
            if (line_indent == indent && all_lines[i].find_first_not_of(" \t}") == std::string::npos) {
                // Found the function's closing brace at the right indentation level
                closing_brace_line = i;
                break;
            }
        }
    }
    
    // Show 6 lines before and including the closing brace
    int pre_nolintend_start = std::max(closing_brace_line - pre_end_lines + 1, warning.line_number + displayed_function_lines);
    for (int i = pre_nolintend_start; i <= closing_brace_line; ++i) {
        if (i >= 0 && i > warning.line_number + post_begin_lines - 1 && i < static_cast<int>(all_lines.size())) {
            result.lines.push_back(std::to_string(i + 1) + ": " + all_lines[i]);
        }
    }
    
    // Add NOLINTEND after the closing brace (on the next line after line 453)
    int nolintend_line = closing_brace_line + 1;
    result.lines.push_back(std::string(std::to_string(nolintend_line).length() + 2, ' ') + indent + "// NOLINTEND(" + warning.type + ")");
    
    // Show 6 lines after NOLINTEND for context
    int post_end_lines = std::min(context_lines * 3, 6); // Scale with context, max 6
    for (int i = nolintend_line; i < std::min(nolintend_line + post_end_lines, static_cast<int>(all_lines.size())); ++i) {
        if (i >= 0 && i < static_cast<int>(all_lines.size())) {
            result.lines.push_back(std::to_string(i + 1) + ": " + all_lines[i]);
        }
    }
    
    return result;
}

// Smart input handling with automatic /dev/tty redirect
struct InputResult {
    std::vector<nolint::Warning> warnings;
    bool stdin_redirected = false;
    std::string status_message;
};

auto handle_smart_input(const Config& config) -> InputResult {
    using namespace nolint;
    InputResult result;
    WarningParser parser;
    
    if (config.use_stdin) {
        auto input_type = detect_input_type();
        
        if (input_type == InputType::TERMINAL) {
            // Normal terminal - no input available
            result.status_message = "No input provided. Usage: clang-tidy file.cpp | nolint";
            return result;
        }
        
        // We have piped or redirected input
        std::cout << "  Detected " << describe_input_type(input_type) << " input - processing...\n";
        
        // STEP 1: Read ALL stdin data immediately (before FTXUI starts)
        std::string all_input;
        std::string line;
        while (std::getline(std::cin, line)) {
            all_input += line + "\n";
        }
        
        // Parse warnings from the input
        result.warnings = parser.parse(all_input);
        
        // STEP 2: Redirect stdin to /dev/tty for keyboard input
        if (config.interactive && !result.warnings.empty()) {
            if (freopen("/dev/tty", "r", stdin)) {
                result.stdin_redirected = true;
                result.status_message = "Keyboard input enabled for interactive mode";
            } else {
                result.status_message = "/dev/tty unavailable - use -i flag for full keyboard support";
            }
        }
        
    } else {
        // File input - no stdin conflict
        std::ifstream file(config.input_file);
        if (!file) {
            result.status_message = "Error: Cannot open file " + config.input_file;
            return result;
        }
        result.warnings = parser.parse(file);
        result.status_message = "Loaded warnings from " + config.input_file;
    }
    
    return result;
}

// Render the UI with dynamic context sizing
auto render_ui(const nolint::UIModel& model, int context_lines = 3) -> ftxui::Element {
    using namespace ftxui;
    using nolint::NolintStyle;
    
    if (model.warnings.empty()) {
        return vbox({
            text("No warnings found") | center,
            separator(),
            text("Press 'q' to quit") | dim
        }) | border;
    }
    
    // Show statistics screen if toggled
    if (model.show_statistics) {
        auto stats = calculate_warning_statistics(model.warnings, model.decisions);
        
        Elements stats_elements;
        stats_elements.push_back(text("  Warning Type Statistics") | bold | center);
        stats_elements.push_back(separator());
        
        // Table header
        stats_elements.push_back(hbox({
            text("  Warning Type") | bold | size(WIDTH, EQUAL, 42),
            text(" Total") | bold | size(WIDTH, EQUAL, 10),
            text(" NOLINT") | bold | size(WIDTH, EQUAL, 10),
            text(" NEXTLINE") | bold | size(WIDTH, EQUAL, 12),
            text(" BLOCK") | bold | size(WIDTH, EQUAL, 10),
            text(" None") | bold | size(WIDTH, EQUAL, 10)
        }) | color(Color::Cyan));
        
        stats_elements.push_back(text("  " + std::string(94, '-')) | color(Color::White));
        
        // Table rows
        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& stat = stats[i];
            bool is_selected = (i == model.statistics_selected_index);
            
            auto row = hbox({
                text("  " + stat.type) | size(WIDTH, EQUAL, 42),
                text(" " + std::to_string(stat.total_count)) | size(WIDTH, EQUAL, 10) | color(Color::White),
                text(" " + std::to_string(stat.nolint_count)) | size(WIDTH, EQUAL, 10) | color(Color::Green),
                text(" " + std::to_string(stat.nolintnextline_count)) | size(WIDTH, EQUAL, 12) | color(Color::Yellow),
                text(" " + std::to_string(stat.nolint_block_count)) | size(WIDTH, EQUAL, 10) | color(Color::Magenta),
                text(" " + std::to_string(stat.unsuppressed_count)) | size(WIDTH, EQUAL, 10) | color(Color::Red)
            });
            
            if (is_selected) {
                row = row | bgcolor(Color::Blue) | bold;
            }
            
            stats_elements.push_back(row);
        }
        
        stats_elements.push_back(separator());
        stats_elements.push_back(text("↑↓: select | Enter: filter | t/Esc: back") | dim);
        
        return vbox(stats_elements) | border;
    }
    
    const auto& warning = model.current_warning();
    
    // Style names for display
    std::vector<std::string> style_names = {"NONE", "NOLINT", "NOLINTNEXTLINE", "NOLINT_BLOCK"};
    auto style_text = style_names[static_cast<int>(model.current_style())];
    
    Elements elements;
    
    // Header with emoji
    elements.push_back(text("  NOLINT Interactive Mode") | bold | center);
    elements.push_back(separator());
    
    // Warning info  
    elements.push_back(hbox({
        text("  File: "), 
        text(warning.file_path + ":" + std::to_string(warning.line_number)) | color(Color::Cyan)
    }));
    elements.push_back(hbox({
        text("  Type: "), 
        text(warning.type) | color(Color::Yellow)
    }));
    elements.push_back(hbox({
        text("  Message: "), 
        text(warning.message)
    }));
    elements.push_back(text(""));
    
    // File context
    elements.push_back(text("  Code Context:") | bold);
    
    // For NOLINT_BLOCK, use a custom balanced context instead of normal context
    if (model.current_style() == NolintStyle::NOLINT_BLOCK && warning.function_lines.has_value()) {
        // Create a balanced NOLINT_BLOCK preview with responsive context sizing
        auto balanced_context = create_balanced_nolint_block_preview(warning, *warning.function_lines, context_lines);
        if (!balanced_context.error_message.empty()) {
            elements.push_back(text(" " + balanced_context.error_message) | color(Color::Red));
        } else {
            for (const auto& line : balanced_context.lines) {
                elements.push_back(text("  " + line) | (line.find("NOLINT") != std::string::npos ? color(Color::Green) : dim));
            }
        }
    } else {
        auto context = nolint::read_file_context(warning, context_lines);
        if (!context.error_message.empty()) {
            elements.push_back(text(" " + context.error_message) | color(Color::Red));
        } else {
            // For NOLINTNEXTLINE, we need to track if we should insert the comment before the warning line
            bool insert_nolintnextline = false;
            std::string nolintnextline_comment;
            
            if (model.current_style() == NolintStyle::NOLINTNEXTLINE) {
                auto preview = nolint::build_suppression_preview(warning, NolintStyle::NOLINTNEXTLINE);
                if (preview) {
                    insert_nolintnextline = true;
                    nolintnextline_comment = *preview;
                }
            }
        
        // If NOLINTNEXTLINE is active, we'll skip the last line to avoid cutting off the warning count
        size_t lines_to_show = context.lines.size();
        if (model.current_style() == NolintStyle::NOLINTNEXTLINE && lines_to_show > 0) {
            lines_to_show--;  // Skip the last line to compensate for the extra NOLINTNEXTLINE comment
        }
        
        for (size_t i = 0; i < lines_to_show; ++i) {
            const auto& line = context.lines[i];
            std::string line_str = std::to_string(line.line_number) + ": " + line.text;
            
            // Check if we need to insert NOLINTNEXTLINE before this line
            if (insert_nolintnextline && line.is_warning_line) {
                // Extract the indentation from the warning line
                std::string indent;
                for (char c : line.text) {
                    if (c == ' ' || c == '\t') {
                        indent += c;
                    } else {
                        break;
                    }
                }
                
                // Insert the NOLINTNEXTLINE comment with matching indentation
                // Need to account for the line number prefix (e.g., "547: ")
                std::string line_prefix = std::to_string(line.line_number) + ": ";
                std::string spaces_for_line_prefix(line_prefix.length(), ' ');
                elements.push_back(text("  " + spaces_for_line_prefix + indent + nolintnextline_comment) | color(Color::Green));
                insert_nolintnextline = false;  // Only insert once
            }
            
            if (line.is_warning_line) {
                if (model.current_style() == NolintStyle::NOLINT) {
                    // Show the modified line with NOLINT comment in green
                    auto preview = nolint::build_suppression_preview(warning, NolintStyle::NOLINT);
                    if (preview) {
                        std::string modified_line = std::to_string(line.line_number) + ": " + line.text + "  " + *preview;
                        elements.push_back(text("  " + modified_line) | color(Color::Green));
                    } else {
                        elements.push_back(text("  " + line_str) | color(Color::Red) | bold);
                    }
                } else if (model.current_style() == NolintStyle::NOLINTNEXTLINE) {
                    // Warning line is shown as normal since it's suppressed by NOLINTNEXTLINE
                    elements.push_back(text("  " + line_str) | dim);
                } else if (model.current_style() == NolintStyle::NONE) {
                    // Show warning line in red when no suppression
                    elements.push_back(text("  " + line_str) | color(Color::Red) | bold);
                } else {
                    // Other styles - just show line normally 
                    elements.push_back(text("  " + line_str) | dim);
                }
            } else {
                elements.push_back(text("  " + line_str) | dim);
            }
        }
        }
    }
    
    elements.push_back(text(""));
    
    // Current suppression style with emoji
    Color style_color = (model.current_style() == NolintStyle::NONE) ? Color::White : Color::Green;
    elements.push_back(hbox({
        text("  Suppression: "), 
        text(style_text) | color(style_color) | bold
    }));
    
    elements.push_back(text(""));
    elements.push_back(separator());
    
    // Status and controls
    auto warning_count_text = "Warning " + std::to_string(model.current_index + 1) + "/" + 
                             std::to_string(model.total_warnings());
    
    // Add filter status if active
    if (!model.search_filter.empty()) {
        warning_count_text += " (filtered: " + model.search_filter + ")";
    }
    
    elements.push_back(hbox({
        text("  " + warning_count_text) | bold,
        text(" | "),
        text("↑↓: style | ←→: nav | /: search | t: stats | x: save | q: quit") | dim
    }));
    
    return vbox(elements) | border;
}

int main(int argc, char* argv[]) {
    using namespace ftxui;
    using namespace nolint;
    
    auto config = parse_args(argc, argv);
    
    // Smart input handling with automatic detection
    auto input_result = handle_smart_input(config);
    
    // Show status message
    if (!input_result.status_message.empty()) {
        std::cout << input_result.status_message << "\n";
    }
    
    if (input_result.warnings.empty()) {
        if (input_result.status_message.find("Error:") != std::string::npos) {
            return 1;
        }
        return 0;
    }
    
    std::cout << "  Found " << input_result.warnings.size() << " warnings.\n";
    
    // Handle non-interactive mode
    if (!config.interactive) {
        std::cout << "  Non-interactive mode: applying NOLINT to all warnings\n";
        
        std::unordered_map<size_t, NolintStyle> decisions;
        for (size_t i = 0; i < input_result.warnings.size(); ++i) {
            decisions[i] = NolintStyle::NOLINT;
        }
        
        FileModifier modifier;
        auto result = modifier.apply_decisions(input_result.warnings, decisions, config.dry_run);
        
        if (result.success) {
            std::cout << "Successfully processed " << result.modified_files.size() << " files\n";
        } else {
            std::cerr << "Errors occurred: " << result.error_message << "\n";
            return 1;
        }
        
        return 0;
    }
    
    // Interactive mode
    std::cout << "\n  Interactive mode - use arrow keys to navigate and select suppressions\n";
    if (config.dry_run) {
        std::cout << "DRY RUN MODE - no files will be modified\n";
    }
    if (input_result.stdin_redirected) {
        std::cout << "Keyboard input active via /dev/tty\n";
    }
    std::cout << "\n";
    
    // Initialize UIModel
    UIModel model;
    model.warnings = input_result.warnings;
    model.dry_run = config.dry_run;
    
    // Initialize with all warnings visible (no filter)
    model.filtered_warning_indices = filter_warnings(model.warnings, "");
    
    auto screen = ScreenInteractive::Fullscreen();
    
    // Create search input component
    std::string search_input_text;
    auto search_input = Input(&search_input_text, "Enter search filter...");
    
    // Create main UI component with dynamic context sizing
    auto main_component = Renderer([&model] {
        // Calculate dynamic context lines based on terminal height
        int terminal_height = ftxui::Terminal::Size().dimy;
        int fixed_ui_lines = 13; // header(2) + warning_info(4) + context_header(1) + suppression(3) + status(2) + border(2) + margins(1)
        
        // Reserve extra space for NOLINT_BLOCK preview (balanced preview is ~12 lines total)
        if (model.current_style() == NolintStyle::NOLINT_BLOCK) {
            fixed_ui_lines += 8; // Reserve extra space for the balanced preview
        } else if (model.current_style() == NolintStyle::NOLINTNEXTLINE) {
            fixed_ui_lines += 1; // NOLINTNEXTLINE adds one extra line
        }
        
        int available_for_code = std::max(7, terminal_height - fixed_ui_lines); // minimum 7 lines
        int context_lines = std::max(2, (available_for_code - 1) / 2); // -1 for warning line, /2 for before+after, minimum 2
        
        return render_ui(model, context_lines);
    });
    
    // Create search UI component
    auto search_component = Renderer(search_input, [&search_input_text] {
        return vbox({
            text("Search Filter:") | bold,
            separator(),
            hbox({
                text("Filter: "),
                text(search_input_text) | color(Color::Cyan)
            }),
            text("Enter to apply, Escape to cancel") | dim
        }) | border;
    });
    
    // Container component that switches between main and search UI
    int ui_selector = MAIN_UI;
    auto component = Container::Tab({
        main_component,
        search_component
    }, &ui_selector);
    
    // Add event handler with direct state mutation (for FTXUI)
    component = component | CatchEvent([&model, &screen, &search_input_text, &ui_selector](Event event) {
        // Handle search mode events
        if (ui_selector == SEARCH_UI) {  // In search mode
            if (event == Event::Return) {
                // Apply search filter
                model.search_filter = search_input_text;
                model.filtered_warning_indices = filter_warnings(model.warnings, model.search_filter);
                model.current_index = 0;  // Reset to first filtered result
                ui_selector = MAIN_UI;  // Return to main UI
                return true;
            } else if (event == Event::Escape) {
                // Cancel search
                ui_selector = MAIN_UI;  // Return to main UI
                search_input_text.clear();
                return true;
            }
            // Let search input handle other events
            return false;
        }
        // Map events to our InputEvent enum
        InputEvent input_event = InputEvent::UNKNOWN;
        
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            input_event = InputEvent::QUIT;
        } else if (event == Event::Character('x') || event == Event::Character('X')) {
            input_event = InputEvent::SAVE_EXIT;
        } else if (event == Event::Character('t') || event == Event::Character('T')) {
            input_event = InputEvent::SHOW_STATISTICS;
        } else if (event == Event::ArrowUp) {
            input_event = InputEvent::ARROW_UP;
        } else if (event == Event::ArrowDown) {
            input_event = InputEvent::ARROW_DOWN;
        } else if (event == Event::ArrowLeft) {
            input_event = InputEvent::ARROW_LEFT;
        } else if (event == Event::ArrowRight) {
            input_event = InputEvent::ARROW_RIGHT;
        } else if (event == Event::Character('/')) {
            input_event = InputEvent::SEARCH;
        } else if (event == Event::Return) {
            input_event = InputEvent::ENTER;
        } else if (event == Event::Escape) {
            input_event = InputEvent::ESCAPE;
        }
        
        if (input_event == InputEvent::UNKNOWN) {
            return false;
        }
        
        // Use our pure update function
        auto new_model = update(model, input_event);
        model = new_model;  // Mutate for FTXUI
        
        // Handle search mode activation
        if (input_event == InputEvent::SEARCH) {
            ui_selector = SEARCH_UI;  // Switch to search UI
            search_input_text.clear();  // Clear previous search
        }
        
        // Exit if needed
        if (model.should_exit) {
            screen.Exit();
            return true;
        }
        
        return true;
    });
    
    // Run the app
    screen.Loop(component);
    
    // Apply decisions when exiting
    if (!model.decisions.empty() && model.should_save) {
        std::cout << "\n  Applying decisions to files...\n";
        
        FileModifier modifier;
        auto result = modifier.apply_decisions(model.warnings, model.decisions, model.dry_run);
        
        if (result.success) {
            std::cout << "Successfully processed " << result.modified_files.size() << " files:\n";
            for (const auto& file : result.modified_files) {
                std::cout << "  ※ " << file << "\n";
            }
        } else {
            std::cerr << "Some errors occurred:\n";
            std::cerr << result.error_message << "\n";
            for (const auto& file : result.failed_files) {
                std::cerr << "  Failed: " << file << "\n";
            }
            return 1;
        }
    } else {

        // Either no NOLINT decisions were made, or specifically not
        // saving
        std::cout << "\n  Exited without saving - no files modified.\n";
    }
    
    return 0;
}
