#pragma once

#include "context_builder.hpp"
#include "file_manager.hpp"
#include "interfaces.hpp"
#include "types.hpp"
#include <memory>

namespace nolint {

// Main processor that coordinates all components
class NolintProcessor {
public:
    NolintProcessor(std::unique_ptr<IWarningParser> parser,
                    std::unique_ptr<FileManager> file_manager, std::unique_ptr<IUserInterface> ui);

    // Process warnings from input stream
    auto process_warnings(std::istream& input) -> void;

private:
    std::unique_ptr<IWarningParser> parser_;
    std::unique_ptr<FileManager> file_manager_;
    std::unique_ptr<IUserInterface> ui_;
    ContextBuilder context_builder_;

    // Create modification from warning
    auto create_modification(const Warning& warning, NolintStyle style) -> Modification;

    // Handle user's choice for a warning
    auto handle_user_action(UserAction action, const Warning& warning)
        -> bool; // true = continue processing

    // Process a single warning interactively
    auto process_single_warning(const Warning& warning, size_t index, size_t total)
        -> bool; // true = continue processing
};

} // namespace nolint