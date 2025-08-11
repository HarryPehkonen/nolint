#pragma once

#include "ui_model.hpp"
#include <string>
#include <vector>
#include <optional>

namespace nolint {

// Represents lines of code with context around a warning
struct FileContext {
    struct Line {
        int line_number = 0;
        std::string text;
        bool is_warning_line = false;
        std::optional<std::string> preview_comment;  // Shows what NOLINT would look like
    };
    
    std::vector<Line> lines;
    std::string error_message;  // Set if file couldn't be read
};

// Read file context around a warning location
auto read_file_context(const Warning& warning, int context_lines = 3) -> FileContext;

// Build preview of what the suppression would look like
auto build_suppression_preview(const Warning& warning, NolintStyle style) -> std::optional<std::string>;

} // namespace nolint