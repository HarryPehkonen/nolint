#include "nolint/context_builder.hpp"
#include <algorithm>
#include <iostream>

namespace nolint {

auto ContextBuilder::build_context(const Warning& warning,
                                   const std::vector<std::string>& file_lines, NolintStyle style,
                                   int context_size) -> WarningContext {
    WarningContext context;
    context.warning = warning;
    context.style = style;
    context.lines = extract_context_lines(file_lines, warning.line_number, context_size);

    return context;
}

auto ContextBuilder::extract_context_lines(const std::vector<std::string>& file_lines,
                                           int warning_line, int context_size)
    -> std::vector<CodeLine> {
    std::vector<CodeLine> lines;

    // Calculate range (1-based line numbers)
    int start_line = std::max(1, warning_line - context_size);
    int end_line = std::min(static_cast<int>(file_lines.size()), warning_line + context_size);

    // Extract lines
    for (int line_num = start_line; line_num <= end_line; ++line_num) {
        CodeLine code_line;
        code_line.number = line_num;
        code_line.text = file_lines[line_num - 1]; // Convert to 0-based index
        lines.push_back(code_line);
    }

    return lines;
}

} // namespace nolint