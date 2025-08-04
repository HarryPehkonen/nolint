#include "nolint/nolint_processor.hpp"
#include "nolint/simple_ui.hpp"
#include <iostream>

namespace nolint {

NolintProcessor::NolintProcessor(std::unique_ptr<IWarningParser> parser,
                                 std::unique_ptr<FileManager> file_manager,
                                 std::unique_ptr<IUserInterface> ui)
    : parser_(std::move(parser)), file_manager_(std::move(file_manager)), ui_(std::move(ui)) {}

auto NolintProcessor::process_warnings(std::istream& input) -> void {
    // Parse all warnings
    auto warnings = parser_->parse(input);

    if (warnings.empty()) {
        std::cout << "No warnings found in input.\n";
        return;
    }

    // Create RAII transaction for file modifications
    FileTransaction transaction(*file_manager_);

    // Process each warning interactively
    for (size_t i = 0; i < warnings.size(); ++i) {
        const auto& warning = warnings[i];

        bool should_continue = process_single_warning(warning, i + 1, warnings.size());
        if (!should_continue) {
            break; // User chose to quit or exit
        }

        // TODO: Handle style changes that require redisplaying the same warning
        // This would require returning the UserAction from process_single_warning
    }

    // Show summary
    ui_->show_summary(file_manager_->get_modified_file_count(),
                      file_manager_->get_total_modifications());

    // Transaction will be committed or rolled back based on user's final choice
    if (file_manager_->get_modified_file_count() > 0) {
        transaction.commit();
        std::cout << "Changes saved.\n";
    }
}

auto NolintProcessor::create_modification(const Warning& warning, NolintStyle style)
    -> Modification {
    Modification mod;
    mod.line_number = warning.line_number;
    mod.style = style;
    mod.warning_type = warning.warning_type;

    // Always extract indentation from the warning line
    // For NOLINTNEXTLINE, this will be used when inserting the line
    // For inline comments, it won't be used but doesn't hurt to have
    mod.indent = file_manager_->extract_indentation(warning.file_path, warning.line_number);

    // For block suppressions, find the boundaries
    if (style == NolintStyle::NOLINT_BLOCK) {
        auto boundaries = file_manager_->find_function_boundaries(warning.file_path, warning);
        mod.block_start_line = boundaries.first;
        mod.block_end_line = boundaries.second;
    }

    return mod;
}

auto NolintProcessor::handle_user_action(UserAction action, const Warning& warning) -> bool {
    switch (action) {
    case UserAction::ACCEPT: {
        // Get current style from UI (need to cast to access the method)
        auto* simple_ui = dynamic_cast<SimpleUI*>(ui_.get());
        NolintStyle current_style
            = simple_ui ? simple_ui->get_current_style() : NolintStyle::NOLINT_SPECIFIC;

        auto modification = create_modification(warning, current_style);
        file_manager_->apply_modification(warning.file_path, modification);
        return true;
    }

    case UserAction::SKIP:
        return true; // Continue to next warning

    case UserAction::QUIT:
        std::cout << "Quitting without saving changes.\n";
        return false; // Stop processing, rollback changes

    case UserAction::EXIT:
        std::cout << "Exiting and saving changes.\n";
        return false; // Stop processing, commit changes

    case UserAction::SAVE:
        file_manager_->write_file(warning.file_path);
        std::cout << "Saved " << warning.file_path << "\n";
        return true; // Continue processing

    case UserAction::STYLE_UP:
    case UserAction::STYLE_DOWN:
        return true; // Continue (will redisplay same warning)

    default:
        return true; // Default: continue
    }
}

auto NolintProcessor::process_single_warning(const Warning& warning, size_t index, size_t total)
    -> bool {
    try {
        // Get file lines
        const auto& file_lines = file_manager_->get_lines(warning.file_path);

        // Get current style from UI
        auto* simple_ui = dynamic_cast<SimpleUI*>(ui_.get());
        NolintStyle current_style
            = simple_ui ? simple_ui->get_current_style() : NolintStyle::NOLINT_SPECIFIC;

        // Build context for display (use smaller context for block mode)
        int context_size = (current_style == NolintStyle::NOLINT_BLOCK) ? 2 : 5;
        auto context
            = context_builder_.build_context(warning, file_lines, current_style, context_size);
        context.current = static_cast<int>(index);
        context.total = static_cast<int>(total);

        // Show warning to user
        ui_->display_context(context);

        // Get user's decision
        auto action = ui_->get_user_action();

        // Handle the action
        return handle_user_action(action, warning);

    } catch (const std::exception& e) {
        std::cerr << "Error processing warning in " << warning.file_path << ":"
                  << warning.line_number << " - " << e.what() << "\n";
        return true; // Continue to next warning
    }
}

} // namespace nolint