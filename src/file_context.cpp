#include "file_context.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace nolint {

auto read_file_context(const Warning& warning, int context_lines) -> FileContext {
    FileContext context;
    
    std::ifstream file(warning.file_path);
    if (!file.is_open()) {
        context.error_message = "Could not open file: " + warning.file_path;
        return context;
    }
    
    // Read all lines
    std::vector<std::string> all_lines;
    std::string line;
    while (std::getline(file, line)) {
        all_lines.push_back(line);
    }
    
    if (warning.line_number < 1 || warning.line_number > static_cast<int>(all_lines.size())) {
        context.error_message = "Line number " + std::to_string(warning.line_number) + " out of range";
        return context;
    }
    
    // Calculate range to show (1-indexed to 0-indexed)
    int target_line = warning.line_number - 1;
    int start = std::max(0, target_line - context_lines);
    int end = std::min(static_cast<int>(all_lines.size()), target_line + context_lines + 1);
    
    // Build context
    for (int i = start; i < end; ++i) {
        FileContext::Line ctx_line;
        ctx_line.line_number = i + 1;  // Back to 1-indexed for display
        ctx_line.text = all_lines[i];
        ctx_line.is_warning_line = (i == target_line);
        context.lines.push_back(ctx_line);
    }
    
    return context;
}

auto build_suppression_preview(const Warning& warning, NolintStyle style) -> std::optional<std::string> {
    switch (style) {
        case NolintStyle::NOLINT:
            return "  // NOLINT(" + warning.type + ")";
            
        case NolintStyle::NOLINTNEXTLINE:
            return "// NOLINTNEXTLINE(" + warning.type + ")";
            
        case NolintStyle::NOLINT_BLOCK:
            return "// NOLINTBEGIN(" + warning.type + ")";
            
        case NolintStyle::NONE:
        default:
            return std::nullopt;
    }
}

} // namespace nolint
