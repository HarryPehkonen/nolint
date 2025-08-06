#include "nolint_app.hpp"
#include "functional_core.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h> // for isatty

namespace nolint {

auto NolintApp::run(const AppConfig& config) -> int {
    config_ = config;

    // Parse warnings from input
    auto warnings = parse_warnings();
    if (warnings.empty()) {
        terminal_->print_line("No warnings found in input.");
        return 0;
    }

    terminal_->print_line("Found " + std::to_string(warnings.size()) + " warnings.");

    // Check if we can be interactive using the terminal's capability detection
    bool can_be_interactive = config_.interactive && terminal_->is_interactive();

    if (!isatty(STDIN_FILENO) && config_.interactive) {
        if (terminal_->is_interactive()) {
            terminal_->print_line("Input is piped - using /dev/tty for interaction.");
        } else {
            terminal_->print_line(
                "Input is piped and /dev/tty unavailable - using non-interactive mode.");
        }
    }

    if (can_be_interactive) {
        // Interactive mode - show each warning and get user decision
        process_warnings(warnings);

        // Apply all decisions
        if (!session_.decisions.empty()) {
            terminal_->print_line("\nApplying modifications...");
            if (apply_decisions()) {
                terminal_->print_line("Successfully applied "
                                      + std::to_string(session_.decisions.size())
                                      + " modifications.");
                return 0;
            } else {
                terminal_->print_line("Error: Failed to apply some modifications.");
                return 1;
            }
        } else {
            terminal_->print_line("No modifications selected.");
            return 0;
        }
    } else {
        // Non-interactive mode - apply default style to all warnings
        for (const auto& warning : warnings) {
            session_.decisions.emplace_back(warning, config_.default_style);
        }

        if (apply_decisions()) {
            terminal_->print_line("Applied default style to " + std::to_string(warnings.size())
                                  + " warnings.");
            return 0;
        } else {
            terminal_->print_line("Error: Failed to apply modifications.");
            return 1;
        }
    }
}

auto NolintApp::parse_warnings() -> std::vector<Warning> {
    if (config_.input_file.has_value()) {
        // Read from file
        std::ifstream file(config_.input_file.value());
        if (!file.is_open()) {
            terminal_->print_line("Error: Cannot open input file: " + config_.input_file.value());
            return {};
        }
        return parser_->parse_warnings(file);
    } else {
        // Read from stdin
        return parser_->parse_warnings(std::cin);
    }
}

auto NolintApp::process_warnings(const std::vector<Warning>& warnings) -> void {
    // Initialize search state
    session_.original_warnings = warnings;
    session_.filtered_warnings = warnings;
    session_.current_filter.clear();

    terminal_->print_line("\n=== Interactive NOLINT Tool ===");
    terminal_->print_line("For each warning, choose:");
    terminal_->print_line("  ↑/↓ - Cycle through suppression styles");
    terminal_->print_line("  ←/→ - Navigate between warnings");
    terminal_->print_line("  x - Save and eXit with summary");
    terminal_->print_line("  q - Quit without saving");
    terminal_->print_line("  / - Search/filter warnings");
    terminal_->print_line("");

    NolintStyle current_style = config_.default_style;
    int current_index = 0;

    while (current_index >= 0 && current_index < static_cast<int>(get_active_warnings().size())) {
        const auto& active_warnings = get_active_warnings();
        const auto& warning = active_warnings[current_index];

        // Check if we have a previous decision for this warning
        std::string warning_key = get_warning_key(warning);
        auto prev_decision = session_.warning_decisions.find(warning_key);
        if (prev_decision != session_.warning_decisions.end()) {
            current_style = prev_decision->second;
        } else {
            // Default to no suppression for unvisited warnings
            current_style = NolintStyle::NONE;
        }

        // Make sure current style is available for this warning
        if (current_style == NolintStyle::NOLINT_BLOCK
            && !functional_core::is_function_level_warning(warning.warning_type)) {
            current_style = NolintStyle::NOLINT_SPECIFIC;
        }

        bool style_chosen = false;
        bool go_previous = false;

        while (!style_chosen && !go_previous) {
            // Clear screen and display warning with current style
            terminal_->print("\033[2J\033[H"); // Clear screen, move cursor to home
            terminal_->print_line("=== Interactive NOLINT Tool ===");

            // Display suppression count
            int suppression_count = count_suppressions();
            terminal_->print_line("Suppressions: " + std::to_string(suppression_count)
                                  + " | Use ←→ to navigate, ↑↓ to change style");

            // Display filter status
            if (session_.current_filter.empty()) {
                terminal_->print_line("Showing " + std::to_string(get_active_warnings().size())
                                      + "/" + std::to_string(session_.original_warnings.size())
                                      + " warnings | Use / to search");
            } else {
                terminal_->print_line("Showing " + std::to_string(get_active_warnings().size())
                                      + "/" + std::to_string(session_.original_warnings.size())
                                      + " warnings (filtered: '" + session_.current_filter
                                      + "') | Use / to search");
            }
            terminal_->print_line("");

            display_warning(warning, current_index + 1, get_active_warnings().size(),
                            current_style);

            auto action = get_user_decision_with_arrows(warning, current_style);

            switch (action) {

            case UserAction::NEXT:
                // Auto-save current choice when navigating
                session_.warning_decisions[warning_key] = current_style;

                if (current_index < static_cast<int>(get_active_warnings().size()) - 1) {
                    current_index++;     // Move to next warning
                    style_chosen = true; // Exit inner loop to process next warning
                } else {
                    terminal_->print_line("Already at last warning.");
                }
                break;

            case UserAction::PREVIOUS:
                // Auto-save current choice when navigating
                session_.warning_decisions[warning_key] = current_style;

                if (current_index > 0) {
                    current_index--;     // Move to previous warning
                    style_chosen = true; // Exit inner loop to process previous warning
                } else {
                    terminal_->print_line("Already at first warning.");
                }
                break;

            case UserAction::SAVE_EXIT: {
                // Auto-save current choice first
                session_.warning_decisions[warning_key] = current_style;

                terminal_->print_line("Saving changes and exiting...");

                // Build final decisions from session_.warning_decisions
                session_.decisions.clear();
                for (const auto& [key, style] : session_.warning_decisions) {
                    // Find the corresponding warning
                    for (const auto& w : warnings) {
                        if (get_warning_key(w) == key && style != NolintStyle::NONE) {
                            session_.decisions.emplace_back(w, style);
                        }
                    }
                }

                if (apply_decisions()) {
                    terminal_->print_line("Successfully applied "
                                          + std::to_string(session_.decisions.size())
                                          + " suppressions.");
                    std::exit(0);
                } else {
                    terminal_->print_line("Error: Failed to apply some modifications.");
                    std::exit(1);
                }
                break;
            }

            case UserAction::QUIT: {
                terminal_->print("Are you sure you want to quit without saving? [y/n]: ");
                char confirm = terminal_->read_char();
                terminal_->print_line(""); // New line after input

                if (std::tolower(confirm) == 'y') {
                    terminal_->print_line("Quitting without saving changes.");
                    std::exit(0);
                } else {
                    terminal_->print_line("Continuing...");
                    // Just continue the loop, don't exit
                }
                break;
            }

            case UserAction::ARROW_KEY:
                // Auto-save current style choice when user changes it with arrow keys
                session_.warning_decisions[warning_key] = current_style;
                // Continue the loop to redisplay with new style
                break;

            case UserAction::SEARCH:
                handle_search_input();
                // After search, we need to adjust current_index if it's out of bounds
                // This is CRITICAL - must be done before continuing the loop!
                if (current_index >= static_cast<int>(get_active_warnings().size())) {
                    current_index = std::max(0, static_cast<int>(get_active_warnings().size()) - 1);
                }
                // Exit the inner loop to restart the outer loop with proper bounds
                style_chosen = true;
                break;
            }
        }
    }
}

auto NolintApp::display_warning(const Warning& warning, size_t index, size_t total,
                                NolintStyle current_style) -> void {

    terminal_->print_line("┌─ Warning " + std::to_string(index) + "/" + std::to_string(total)
                          + " ─");
    terminal_->print_line("│ File: " + warning.file_path);
    terminal_->print_line("│ Line: " + std::to_string(warning.line_number) + ":"
                          + std::to_string(warning.column_number));
    terminal_->print_line("│ Type: " + warning.warning_type);
    terminal_->print_line("│ Message: " + warning.message);

    if (warning.function_lines.has_value()) {
        terminal_->print_line("│ Function size: " + std::to_string(warning.function_lines.value())
                              + " lines");
    }

    // Load and display context with integrated preview
    auto file_lines = load_file(warning.file_path);

    if (!file_lines.empty()) {
        auto context = functional_core::build_display_context(warning, file_lines, current_style);
        terminal_->print_line("│");

        // Display the context lines with integrated NOLINT preview
        for (const auto& line : context.context_lines) {
            terminal_->print_line("│ " + line);
        }

        terminal_->print_line("│");
        terminal_->print_line("│ Apply NOLINT? Format: " + context.format_preview);
    }

    terminal_->print_line("└─");
}

auto NolintApp::get_user_decision_with_arrows(const Warning& warning, NolintStyle& current_style)
    -> UserAction {
    int empty_attempts = 0;
    const int max_empty_attempts = 5; // Prevent infinite loop in non-interactive environments

    while (true) {
        terminal_->print("Navigate [←→] Style [↑↓] Save & Exit [x] Quit [q] Search [/]: ");

        // Use read_char for immediate single-key response
        char input = terminal_->read_char();

        if (input == 0 || input == EOF) {
            empty_attempts++;
            if (empty_attempts >= max_empty_attempts) {
                terminal_->print_line("\nNo input received. Skipping remaining warnings.");
                std::exit(0);
            }
            continue;
        }

        empty_attempts = 0; // Reset counter on valid input

        auto action = parse_input_char(input, warning, current_style);
        if (action != UserAction::ARROW_KEY) {
            // Valid action received (not just style cycling or invalid input)
            return action;
        }
        // Continue loop for ARROW_KEY (style changed) or invalid input
    }
}

auto NolintApp::parse_input_char(char c, const Warning& warning, NolintStyle& current_style)
    -> UserAction {
    // Handle arrow keys and escape sequences
    if (c == 27) { // ESC key - check for arrow sequence
        char next1 = terminal_->read_char();
        if (next1 == '[') {
            char arrow = terminal_->read_char();
            if (arrow == 'A') { // Up arrow
                current_style = cycle_style(current_style, warning, true);
                terminal_->print_line(""); // New line after key press
                return UserAction::ARROW_KEY;
            } else if (arrow == 'B') { // Down arrow
                current_style = cycle_style(current_style, warning, false);
                terminal_->print_line(""); // New line after key press
                return UserAction::ARROW_KEY;
            } else if (arrow == 'C') {     // Right arrow - next warning
                terminal_->print_line(""); // New line after key press
                return UserAction::NEXT;
            } else if (arrow == 'D') {     // Left arrow - previous warning
                terminal_->print_line(""); // New line after key press
                return UserAction::PREVIOUS;
            }
        }
        // Invalid escape sequence - return ARROW_KEY to continue loop
        return UserAction::ARROW_KEY;
    }

    char choice = std::tolower(c);
    terminal_->print_line(""); // New line after key press

    switch (choice) {
    case 'x':
        return UserAction::SAVE_EXIT;
    case 'q':
        return UserAction::QUIT;
    case '/':
        return UserAction::SEARCH;
    default:
        terminal_->print_line("Invalid choice. Use ←→ to navigate, ↑↓ to change style, "
                              "'x' to save & exit, 'q' to quit, '/' to search.");
        return UserAction::ARROW_KEY; // Continue loop
    }
}

auto NolintApp::apply_decisions() -> bool {
    if (config_.dry_run) {
        terminal_->print_line("Dry run mode - no files will be modified.");
        return true;
    }

    // Group decisions by file
    std::unordered_map<std::string, std::vector<std::pair<Warning, NolintStyle>>> file_decisions;

    for (const auto& [warning, style] : session_.decisions) {
        file_decisions[warning.file_path].emplace_back(warning, style);
    }

    // Process each file
    bool all_success = true;
    for (const auto& [file_path, file_warnings] : file_decisions) {
        auto original_lines = load_file(file_path);
        if (original_lines.empty()) {
            terminal_->print_line("Warning: Could not read file " + file_path);
            all_success = false;
            continue;
        }

        // Create modifications for this file
        std::vector<Modification> modifications;
        for (const auto& [warning, style] : file_warnings) {
            auto mod = functional_core::create_modification(warning, style, original_lines);
            if (mod.style != NolintStyle::NONE) {
                modifications.push_back(mod);
            }
        }

        // Sort modifications by line number (reverse order for correct line tracking)
        std::sort(modifications.begin(), modifications.end(),
                  [](const auto& a, const auto& b) { return a.target_line > b.target_line; });

        // Apply modifications
        auto current_lines = original_lines;
        for (const auto& mod : modifications) {
            auto result = functional_core::apply_modification_to_lines(current_lines, mod);
            current_lines = std::move(result.lines);
        }

        // Save modified file
        if (!save_file(file_path, current_lines)) {
            terminal_->print_line("Error: Could not write file " + file_path);
            all_success = false;
        }
    }

    return all_success;
}

auto NolintApp::load_file(const std::string& path) -> std::vector<std::string> {
    // Check cache first
    auto it = session_.file_cache.find(path);
    if (it != session_.file_cache.end()) {
        return it->second;
    }

    // Load from filesystem
    auto lines = filesystem_->read_file(path);
    session_.file_cache[path] = lines;
    return lines;
}

auto NolintApp::save_file(const std::string& path, std::span<const std::string> lines) -> bool {
    return filesystem_->write_file(path, lines);
}

auto NolintApp::build_context_display(const Warning& warning,
                                      std::span<const std::string> file_lines) -> std::string {
    auto context
        = functional_core::build_display_context(warning, file_lines, NolintStyle::NOLINT_SPECIFIC);

    std::ostringstream output;
    for (size_t i = 0; i < context.context_lines.size(); ++i) {
        const auto& line = context.context_lines[i];
        int line_number = warning.line_number - context.warning_line_index + static_cast<int>(i);
        output << "│ " << std::setw(4) << line_number << " │ " << line << '\n';
    }

    return output.str();
}

auto NolintApp::build_context_with_preview(const DisplayContext& context, const Warning& warning)
    -> std::string {
    std::ostringstream output;
    for (size_t i = 0; i < context.context_lines.size(); ++i) {
        const auto& line = context.context_lines[i];
        int line_number = warning.line_number - context.warning_line_index + static_cast<int>(i);

        // Mark the warning line with ">>"
        if (static_cast<int>(i) == context.warning_line_index) {
            output << "│ " << std::setw(4) << line_number << " │ >> " << line << '\n';
        } else {
            output << "│ " << std::setw(4) << line_number << " │    " << line << '\n';
        }
    }
    return output.str();
}

auto NolintApp::get_style_name(NolintStyle style) -> std::string {
    switch (style) {
    case NolintStyle::NOLINT_SPECIFIC:
        return "Specific";
    case NolintStyle::NOLINTNEXTLINE:
        return "Next Line";
    case NolintStyle::NOLINT_BLOCK:
        return "Block";
    case NolintStyle::NONE:
        return "No Suppression";
    }
    return "Unknown";
}

auto NolintApp::cycle_style(NolintStyle current, const Warning& warning, bool up) -> NolintStyle {
    std::vector<NolintStyle> available_styles
        = {NolintStyle::NOLINT_SPECIFIC, NolintStyle::NOLINTNEXTLINE, NolintStyle::NONE};

    // Add NOLINT_BLOCK only for function-level warnings
    if (functional_core::is_function_level_warning(warning.warning_type)) {
        available_styles.insert(available_styles.end() - 1, NolintStyle::NOLINT_BLOCK);
    }

    // Find current style index
    auto it = std::find(available_styles.begin(), available_styles.end(), current);
    if (it == available_styles.end()) {
        return available_styles[0]; // Default to first style
    }

    size_t current_index = std::distance(available_styles.begin(), it);

    if (up) {
        // Move to previous style (wrap around)
        current_index = (current_index == 0) ? available_styles.size() - 1 : current_index - 1;
    } else {
        // Move to next style (wrap around)
        current_index = (current_index + 1) % available_styles.size();
    }

    return available_styles[current_index];
}

auto NolintApp::get_warning_key(const Warning& warning) -> std::string {
    return warning.file_path + ":" + std::to_string(warning.line_number) + ":"
           + std::to_string(warning.column_number);
}

auto NolintApp::count_suppressions() const -> int {
    int count = 0;
    for (const auto& [key, style] : session_.warning_decisions) {
        if (style != NolintStyle::NONE) {
            count++;
        }
    }
    return count;
}

auto NolintApp::handle_search_input() -> void {
    terminal_->print_line(""); // New line after key press

    // Show current filter if active
    if (session_.current_filter.empty()) {
        terminal_->print("Enter search filter (empty to clear): ");
    } else {
        terminal_->print("Enter search filter (current: '" + session_.current_filter
                         + "', empty to clear): ");
    }

    std::string new_filter = terminal_->read_line();
    apply_filter(new_filter);
}

auto NolintApp::apply_filter(const std::string& filter) -> void {
    if (filter.empty()) {
        // Clear filter - show all warnings
        session_.current_filter.clear();
        session_.filtered_warnings = session_.original_warnings;
        terminal_->print_line("Filter cleared - showing all "
                              + std::to_string(session_.original_warnings.size()) + " warnings");
    } else {
        // Apply new filter
        session_.current_filter = filter;
        session_.filtered_warnings
            = functional_core::filter_warnings(session_.original_warnings, filter);

        if (session_.filtered_warnings.empty()) {
            // No matches - revert to showing all warnings
            session_.filtered_warnings = session_.original_warnings;
            session_.current_filter.clear();
            terminal_->print_line("No warnings match filter '" + filter + "' - showing all "
                                  + std::to_string(session_.original_warnings.size())
                                  + " warnings");
        } else {
            terminal_->print_line("Applied filter: '" + filter + "' - showing "
                                  + std::to_string(session_.filtered_warnings.size()) + "/"
                                  + std::to_string(session_.original_warnings.size())
                                  + " warnings");
        }
    }
}

auto NolintApp::get_active_warnings() const -> const std::vector<Warning>& {
    return session_.filtered_warnings.empty() ? session_.original_warnings
                                              : session_.filtered_warnings;
}

} // namespace nolint
