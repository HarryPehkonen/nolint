#include "nolint/application/nolint_app.hpp"
#include "nolint/ui/ftxui_terminal.hpp"
#include "nolint/core/functional_core.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace nolint {

NolintApp::NolintApp(std::unique_ptr<ITerminal> terminal, std::unique_ptr<IFileSystem> filesystem,
                     std::unique_ptr<IWarningParser> parser)
    : terminal_(std::move(terminal)), filesystem_(std::move(filesystem)),
      parser_(std::move(parser)) {}

auto NolintApp::run(const Config& config) -> int {
    // Load warnings from input (FIXED: returns warnings instead of storing them)
    auto warnings = load_warnings(config);
    if (warnings.empty()) {
        std::cout << "No warnings found.\n";
        return 0;
    }

    std::cout << "Found " << warnings.size() << " warnings.\n";

    // Initialize UIModel with all state (FIXED: Single source of truth)
    UIModel model;
    model.warnings = std::move(warnings);
    
    // Load existing decisions if specified
    if (!config.load_session_file.empty()) {
        if (auto loaded_decisions = load_decisions(config.load_session_file)) {
            model.decisions = *loaded_decisions;
            std::cout << "Loaded " << model.decisions.size() << " decisions from " 
                     << config.load_session_file << "\n";
        } else {
            std::cerr << "Warning: Could not load decisions from " 
                     << config.load_session_file << "\n";
        }
    }

    if (config.interactive && terminal_->is_interactive()) {
        // Setup terminal for interactive mode
        if (!terminal_->setup_raw_mode()) {
            std::cerr << "Warning: Failed to setup interactive mode, falling back to batch mode\n";
            // TODO: Implement batch mode
            return 1;
        }

        // Try reactive mode first (FTXUI), fall back to legacy polling mode
        auto warnings_copy = model.warnings;
        Decisions final_decisions;
        
        // Check if terminal supports reactive mode (FTXUI)
        if (auto ftxui_terminal = dynamic_cast<FTXUITerminal*>(terminal_.get())) {
            // Use FTXUI reactive mode with your working pattern
            auto final_model = ftxui_terminal->run_reactive_session(model, 
                [this](UIModel m, InputEvent e) { return update(std::move(m), e); }
            );
            final_decisions = final_model.decisions;
        } else {
            // Fall back to legacy polling mode (Terminal)
            final_decisions = run_interactive(std::move(model));
        }

        // Restore terminal
        terminal_->restore_terminal_state();

        // Save session if specified
        if (!config.save_session_file.empty()) {
            if (save_decisions(final_decisions, config.save_session_file)) {
                std::cout << "Saved " << final_decisions.size() << " decisions to " 
                         << config.save_session_file << "\n";
            } else {
                std::cerr << "Warning: Could not save decisions to " 
                         << config.save_session_file << "\n";
            }
        }

        // Apply decisions to files (FIXED: use warnings_copy)
        if (!config.dry_run && !final_decisions.empty()) {
            if (apply_decisions_to_files(final_decisions, warnings_copy)) {
                show_summary(final_decisions);
            } else {
                std::cerr << "Error: Failed to apply some changes\n";
                return 1;
            }
        } else if (config.dry_run) {
            std::cout << "Dry run - no files modified. " << final_decisions.size() << " decisions made.\n";
        }
    } else {
        // Batch mode: automatically apply default style to all warnings
        std::cout << "Running in batch mode with style: " 
                 << style_display_name(config.default_style) << "\n";
        
        auto decisions = run_batch_mode(model, config);
        
        // Save session if specified
        if (!config.save_session_file.empty()) {
            if (save_decisions(decisions, config.save_session_file)) {
                std::cout << "Saved " << decisions.size() << " decisions to " 
                         << config.save_session_file << "\n";
            } else {
                std::cerr << "Warning: Could not save decisions to " 
                         << config.save_session_file << "\n";
            }
        }

        // Apply decisions to files
        if (!config.dry_run && !decisions.empty()) {
            if (apply_decisions_to_files(decisions, model.warnings)) {
                show_summary(decisions);
            } else {
                std::cerr << "Error: Failed to apply some changes\n";
                return 1;
            }
        } else if (config.dry_run) {
            std::cout << "Dry run - no files modified. " << decisions.size() << " decisions made.\n";
        }
    }

    return 0;
}

auto NolintApp::run_interactive(UIModel model) -> Decisions {
    // FIXED: Pure functional reactive loop - no side effects!
    while (model.mode != ViewMode::EXIT) {
        // 1. Render current state (FIXED: only needs model)
        auto screen = render(model);
        terminal_->display_screen(screen);

        // Mark current warning as visited (FIXED: part of model update)
        if (!model.warnings.empty() && model.current_index < model.get_active_warning_count()) {
            size_t actual_index = model.get_actual_warning_index();
            if (actual_index < model.warnings.size()) {
                model.visited_warnings.insert(warning_key(model.warnings[actual_index]));
            }
        }

        // 2. Get input
        auto input = terminal_->get_input_event();

        // 3. Update state (FIXED: pure function with all state in model)
        model = update(model, input);

        // NO MORE SIDE EFFECTS! All state changes are inside update()
    }

    return model.decisions;
}

auto NolintApp::update(UIModel model, InputEvent event) -> UIModel {
    // FIXED: Pure function - only uses parameters, no class member access!
    
    // Reset quit confirmation if user presses any key other than QUIT
    if (event != InputEvent::QUIT && model.quit_confirmation_needed) {
        model.quit_confirmation_needed = false;
        model.status_message = ""; // Clear the quit confirmation message
    }
    
    switch (event) {
    case InputEvent::ARROW_LEFT:
    case InputEvent::ARROW_RIGHT:
        model = functional_core::update_navigation(model, event);
        // NOTE: No need to "restore style" - it's always derived from decisions via get_current_style()
        break;

    case InputEvent::ARROW_UP:
        if (model.mode == ViewMode::REVIEWING) {
            // Arrow navigation in review mode
            if (!model.warnings.empty() && model.current_index < model.get_active_warning_count()) {
                size_t actual_index = model.get_actual_warning_index();
                if (actual_index < model.warnings.size()) {
                    // Up arrow cycles up through styles
                    auto current_style = model.get_current_style();
                    auto new_style = cycle_style_up(current_style, model.warnings[actual_index]);
                    
                    auto key = warning_key(model.warnings[actual_index]);
                    model.decisions[key] = new_style;
                }
            }
        } else if (model.mode == ViewMode::STATISTICS) {
            // Arrow navigation in statistics mode
            if (model.current_stats_index > 0) {
                model.current_stats_index--;
            }
        }
        break;

    case InputEvent::ARROW_DOWN:
        if (model.mode == ViewMode::REVIEWING) {
            // Arrow navigation in review mode
            if (!model.warnings.empty() && model.current_index < model.get_active_warning_count()) {
                size_t actual_index = model.get_actual_warning_index();
                if (actual_index < model.warnings.size()) {
                    // Down arrow cycles down through styles (reverse direction)
                    auto current_style = model.get_current_style();
                    auto new_style = cycle_style_down(current_style, model.warnings[actual_index]);
                    
                    auto key = warning_key(model.warnings[actual_index]);
                    model.decisions[key] = new_style;
                }
            }
        } else if (model.mode == ViewMode::STATISTICS) {
            // Arrow navigation in statistics mode
            if (!model.warning_stats.empty() && model.current_stats_index < model.warning_stats.size() - 1) {
                model.current_stats_index++;
            }
        }
        break;

    case InputEvent::SEARCH:
        if (model.mode == ViewMode::REVIEWING) {
            model.mode = ViewMode::SEARCHING;
            model.search_input.clear();
        }
        break;

    case InputEvent::SHOW_STATISTICS:
        if (model.mode == ViewMode::REVIEWING) {
            model.mode = ViewMode::STATISTICS;
            model.current_stats_index = 0;
            // FIXED: Use model.decisions, not this->decisions_
            model.warning_stats = functional_core::calculate_statistics(model.warnings, 
                                                                       model.decisions,
                                                                       model.visited_warnings);
        } else if (model.mode == ViewMode::STATISTICS) {
            model.mode = ViewMode::REVIEWING;
        }
        break;

    case InputEvent::ESCAPE:
        if (model.mode == ViewMode::SEARCHING || model.mode == ViewMode::STATISTICS) {
            model.mode = ViewMode::REVIEWING;
        }
        break;

    case InputEvent::SAVE_EXIT:
        model.mode = ViewMode::EXIT;
        break;

    case InputEvent::QUIT:
        if (handle_quit_confirmation(model)) {
            model.mode = ViewMode::EXIT;
            model.decisions.clear(); // Don't save decisions when quitting (FIXED)
        }
        break;

    default:
        break;
    }

    return model;
}

auto NolintApp::render(const UIModel& model) -> Screen {
    // FIXED: Only uses model as single source of truth
    switch (model.mode) {
    case ViewMode::REVIEWING:
        return compose_review_screen(model);
    case ViewMode::STATISTICS:
        return compose_statistics_screen(model);
    case ViewMode::SEARCHING:
        return compose_search_screen(model);
    case ViewMode::EXIT:
        return Screen{};
    }
    return Screen{};
}

auto NolintApp::compose_review_screen(const UIModel& model) -> Screen {
    // FIXED: Only uses model
    Screen screen;

    if (model.warnings.empty()) {
        screen.content.push_back(Line{.text = "No warnings to review."});
        screen.status_line = "No warnings found";
        screen.control_hints = "Press 'q' to quit";
        return screen;
    }

    size_t active_count = model.get_active_warning_count();
    if (model.current_index >= active_count) {
        screen.content.push_back(Line{.text = "Invalid warning index."});
        return screen;
    }

    size_t actual_index = model.get_actual_warning_index();
    if (actual_index >= model.warnings.size()) {
        screen.content.push_back(Line{.text = "Invalid warning index."});
        return screen;
    }

    const auto& warning = model.warnings[actual_index];

    // Header
    screen.content.push_back(Line{.text = "=== Interactive NOLINT Tool ==="});
    screen.content.push_back(Line{.text = ""});

    // Warning info
    screen.content.push_back(Line{.text = "┌─ Warning " + std::to_string(model.current_index + 1)
                                          + "/" + std::to_string(active_count) + " ─"});
    screen.content.push_back(Line{.text = "│ File: " + warning.file_path});
    screen.content.push_back(Line{.text = "│ Line: " + std::to_string(warning.line_number) + ":"
                                          + std::to_string(warning.column_number)});
    screen.content.push_back(Line{.text = "│ Type: " + warning.warning_type});
    screen.content.push_back(Line{.text = "│ Message: " + warning.message});
    screen.content.push_back(Line{.text = "│"});

    // Add file context display
    try {
        auto annotated_file = filesystem_->read_file_to_annotated(warning.file_path);
        
        // Extract plain text lines for context building
        std::vector<std::string> file_lines;
        file_lines.reserve(annotated_file.lines.size());
        for (const auto& line : annotated_file.lines) {
            file_lines.push_back(line.text);
        }
        
        auto context = functional_core::build_display_context(warning, file_lines, model.get_current_style());
        
        if (!context.context_lines.empty()) {
            screen.content.push_back(Line{.text = "│ Context:"});
            for (const auto& line : context.context_lines) {
                screen.content.push_back(Line{.text = "│ " + line});
            }
        } else {
            screen.content.push_back(Line{.text = "│ (Could not load file context)"});
        }
    } catch (const std::exception& e) {
        screen.content.push_back(Line{.text = "│ (Error loading file: " + std::string(e.what()) + ")"});
    }
    screen.content.push_back(Line{.text = "│"});

    // Current style (derived from decisions - no redundant state!)
    screen.content.push_back(
        Line{.text = "│ Apply NOLINT? Format: " + style_display_name(model.get_current_style())});
    screen.content.push_back(Line{.text = "└─"});

    // Status line (FIXED: use model.decisions)
    size_t suppression_count
        = std::count_if(model.decisions.begin(), model.decisions.end(),
                        [](const auto& pair) { return pair.second != NolintStyle::NONE; });

    if (model.quit_confirmation_needed) {
        screen.status_line = model.status_message;
    } else if (model.show_boundary_message) {
        screen.status_line = model.status_message;
    } else if (!model.filtered_indices.empty()) {
        screen.status_line = "Showing " + std::to_string(model.filtered_indices.size()) + "/"
                             + std::to_string(model.warnings.size()) + " warnings (filtered: '"
                             + model.search_input + "')";
    } else {
        screen.status_line = "Suppressions: " + std::to_string(suppression_count) + " | Warning "
                             + std::to_string(model.current_index + 1) + "/"
                             + std::to_string(active_count);
    }

    // Control hints
    screen.control_hints = "Navigate [←→] Style [↑↓] Save & Exit [x] Quit [q] Search [/] Stats [t]";

    return screen;
}

auto NolintApp::compose_statistics_screen(const UIModel& model) -> Screen {
    // FIXED: Only uses model
    Screen screen;

    screen.content.push_back(Line{.text = "=== Warning Type Summary ==="});

    // FIXED: Use model.warnings and model.decisions  
    size_t total_warnings = model.warnings.size();
    size_t addressed_count
        = std::count_if(model.decisions.begin(), model.decisions.end(),
                        [](const auto& pair) { return pair.second != NolintStyle::NONE; });
    size_t visited_count = model.visited_warnings.size();

    screen.content.push_back(
        Line{.text = "Total: " + std::to_string(total_warnings)
                     + " warnings | Addressed: " + std::to_string(addressed_count) + " ("
                     + std::to_string(addressed_count * 100 / std::max(total_warnings, size_t(1)))
                     + "%)" + " | Visited: " + std::to_string(visited_count)});
    screen.content.push_back(Line{.text = ""});

    // Table header
    screen.content.push_back(
        Line{.text = "┌─────────────────────────────────────┬─────────┬─────────────┬─────────┐"});
    screen.content.push_back(
        Line{.text = "│ Warning Type                        │  Total  │  Addressed  │ Visited │"});
    screen.content.push_back(
        Line{.text = "├─────────────────────────────────────┼─────────┼─────────────┼─────────┤"});

    // Table content
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

        screen.content.push_back(Line{.text = line});
    }

    screen.content.push_back(
        Line{.text = "└─────────────────────────────────────┴─────────┴─────────────┴─────────┘"});

    screen.status_line = "Statistics Mode";
    screen.control_hints = "Navigate [↑↓] Filter [Enter] Back [Escape]";

    return screen;
}

auto NolintApp::compose_search_screen(const UIModel& model) -> Screen {
    Screen screen;

    screen.content.push_back(Line{.text = "=== Search / Filter Warnings ==="});
    screen.content.push_back(Line{.text = ""});
    screen.content.push_back(Line{.text = "Enter search terms (space-separated for AND logic):"});
    screen.content.push_back(
        Line{.text = "Searches across: file path, warning type, message, line numbers"});
    screen.content.push_back(Line{.text = ""});
    screen.content.push_back(Line{.text = "Current filter: '" + model.search_input + "'"});

    screen.status_line = "Search Mode - Enter search terms, then press Enter";
    screen.control_hints = "Type search terms, [Enter] to apply, [Escape] to cancel";

    return screen;
}

auto NolintApp::load_warnings(const Config& config) -> std::vector<Warning> {
    // FIXED: Returns warnings instead of storing them
    std::string input;

    if (config.input_file == "-") {
        // Read from stdin
        std::string line;
        std::ostringstream oss;
        while (std::getline(std::cin, line)) {
            oss << line << '\n';
        }
        input = oss.str();
    } else {
        // Read from file
        std::ifstream file(config.input_file);
        if (!file.is_open()) {
            return {}; // Return empty vector on error
        }

        std::ostringstream oss;
        oss << file.rdbuf();
        input = oss.str();
    }

    return parser_->parse_warnings(input);
}

auto NolintApp::apply_decisions_to_files(const Decisions& decisions, const std::vector<Warning>& warnings) -> bool {
    // FIXED: Now receives warnings as parameter instead of using member variable
    // Group decisions by file
    std::map<std::string, std::vector<std::pair<Warning, NolintStyle>>> files_to_modify;

    for (const auto& [key, style] : decisions) {
        if (style == NolintStyle::NONE) {
            continue; // Skip non-suppressions
        }

        // Find the warning for this key (FIXED: use parameter warnings)
        auto warning_it = std::find_if(warnings.begin(), warnings.end(),
                                       [&key](const Warning& w) { return warning_key(w) == key; });

        if (warning_it != warnings.end()) {
            files_to_modify[warning_it->file_path].emplace_back(*warning_it, style);
        }
    }

    // Apply changes to each file
    bool all_success = true;
    for (const auto& [file_path, file_decisions] : files_to_modify) {
        try {
            auto annotated_file = filesystem_->read_file_to_annotated(file_path);

            // Apply all decisions for this file
            for (const auto& [warning, style] : file_decisions) {
                functional_core::apply_decision(annotated_file, warning, style);
            }

            // Write the modified file
            if (!filesystem_->write_annotated_file(annotated_file, file_path)) {
                std::cerr << "Error: Failed to write " << file_path << '\n';
                all_success = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << file_path << ": " << e.what() << '\n';
            all_success = false;
        }
    }

    return all_success;
}

auto NolintApp::show_summary(const Decisions& decisions) -> void {
    size_t suppression_count
        = std::count_if(decisions.begin(), decisions.end(),
                        [](const auto& pair) { return pair.second != NolintStyle::NONE; });

    std::cout << "Successfully applied " << suppression_count << " suppressions.\n";
}

auto NolintApp::handle_quit_confirmation(UIModel& model) -> bool {
    // Check if there are any unsaved decisions
    bool has_unsaved_changes = !model.decisions.empty();
    
    if (!has_unsaved_changes) {
        // No unsaved changes, quit immediately
        return true;
    }
    
    if (!model.quit_confirmation_needed) {
        // First time user pressed quit - show confirmation prompt
        model.quit_confirmation_needed = true;
        size_t decision_count = model.decisions.size();
        model.status_message = "Quit without saving " + std::to_string(decision_count) 
                             + " decisions? Press 'q' again to confirm, any other key to cancel";
        return false;
    }

    // User pressed quit twice - confirm the quit
    return true;
}

auto NolintApp::run_batch_mode(const UIModel& model, const Config& config) -> Decisions {
    Decisions decisions;
    
    // Apply default style to all warnings that support it
    for (const auto& warning : model.warnings) {
        if (is_style_available(config.default_style, warning)) {
            auto key = warning_key(warning);
            decisions[key] = config.default_style;
        } else {
            // If default style not available, try NOLINT_SPECIFIC as fallback
            if (is_style_available(NolintStyle::NOLINT_SPECIFIC, warning)) {
                auto key = warning_key(warning);
                decisions[key] = NolintStyle::NOLINT_SPECIFIC;
            }
        }
    }
    
    std::cout << "Processed " << model.warnings.size() << " warnings, created " 
              << decisions.size() << " suppressions.\n";
    
    return decisions;
}

} // namespace nolint