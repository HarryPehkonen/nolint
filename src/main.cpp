// Final version with automatic piped input detection and /dev/tty redirect
#include "ui_model.hpp"
#include "file_context.hpp"
#include "warning_parser.hpp"
#include "file_modifier.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>
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

// Render the UI (same as before, but cleaner)
auto render_ui(const nolint::UIModel& model) -> ftxui::Element {
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
        std::unordered_map<std::string, int> type_counts;
        for (const auto& w : model.warnings) {
            type_counts[w.type]++;
        }
        
        Elements stats_elements;
        stats_elements.push_back(text("  Warning Statistics") | bold | center);
        stats_elements.push_back(separator());
        
        for (const auto& [type, count] : type_counts) {
            stats_elements.push_back(hbox({
                text("• " + type) | color(Color::Yellow),
                text(": "),
                text(std::to_string(count)) | color(Color::Green)
            }));
        }
        
        stats_elements.push_back(separator());
        stats_elements.push_back(text("Press 't' to return to warnings") | dim);
        
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
        text("⚠  Type: "), 
        text(warning.type) | color(Color::Yellow)
    }));
    elements.push_back(hbox({
        text("  Message: "), 
        text(warning.message)
    }));
    elements.push_back(text(""));
    
    // File context
    elements.push_back(text("  Code Context:") | bold);
    
    auto context = nolint::read_file_context(warning, 3);
    if (!context.error_message.empty()) {
        elements.push_back(text(" " + context.error_message) | color(Color::Red));
    } else {
        for (const auto& line : context.lines) {
            std::string line_str = std::to_string(line.line_number) + ": " + line.text;
            
            if (line.is_warning_line) {
                elements.push_back(text(" " + line_str) | color(Color::Red) | bold);
                
                // Show preview if style selected
                auto preview = nolint::build_suppression_preview(warning, model.current_style());
                if (preview && model.current_style() == NolintStyle::NOLINTNEXTLINE) {
                    elements.push_back(text("  ★ " + *preview) | color(Color::Green) | dim);
                } else if (preview && model.current_style() == NolintStyle::NOLINT) {
                    elements.push_back(text("  ★ " + line.text + *preview) | color(Color::Green) | dim);
                }
            } else {
                elements.push_back(text("  " + line_str) | dim);
            }
        }
    }
    
    elements.push_back(text(""));
    
    // Current suppression style with emoji
    std::string style_emoji = (model.current_style() == NolintStyle::NONE) ? "✗" : "✓";
    Color style_color = (model.current_style() == NolintStyle::NONE) ? Color::White : Color::Green;
    elements.push_back(hbox({
        text(style_emoji + " Suppression: "), 
        text(style_text) | color(style_color) | bold
    }));
    
    elements.push_back(text(""));
    elements.push_back(separator());
    
    // Status and controls
    elements.push_back(hbox({
        text("  Warning " + std::to_string(model.current_index + 1) + "/" + 
             std::to_string(model.total_warnings())) | bold,
        text(" | "),
        text("↑↓: style | ←→: navigate | t: stats | x: save & exit | q: quit") | dim
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
    
    auto screen = ScreenInteractive::Fullscreen();
    
    // Create component
    auto component = Renderer([&model] {
        return render_ui(model);
    });
    
    // Add event handler with direct state mutation (for FTXUI)
    component = component | CatchEvent([&model, &screen](Event event) {
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
        }
        
        if (input_event == InputEvent::UNKNOWN) {
            return false;
        }
        
        // Use our pure update function
        auto new_model = update(model, input_event);
        model = new_model;  // Mutate for FTXUI
        
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
    if (!model.decisions.empty()) {
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
        std::cout << "\n  No decisions made - no files modified.\n";
    }
    
    return 0;
}
