#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nolint {

// Represents a warning from clang-tidy
struct Warning {
    std::string file_path;
    int line_number;
    int column_number;
    std::string warning_type;  // e.g., "readability-magic-numbers"
    std::string message;
    std::optional<int> function_lines;  // For function-size warnings
};

// User action choices
enum class UserAction {
    ACCEPT,      // Y - Apply suppression
    SKIP,        // N - Skip this warning
    QUIT,        // Q - Quit without saving
    EXIT,        // X - Exit and save all
    SAVE,        // S - Save current file
    STYLE_UP,    // ↑ - Previous style (future)
    STYLE_DOWN   // ↓ - Next style (future)
};

// NOLINT comment styles
enum class NolintStyle {
    NOLINT,              // // NOLINT
    NOLINT_SPECIFIC,     // // NOLINT(warning-type)
    NOLINTNEXTLINE,      // // NOLINTNEXTLINE(warning-type)
    NOLINT_BLOCK         // // NOLINTBEGIN/END(warning-type)
};

// Line ending style
enum class LineEnding {
    LF,     // Unix/Linux/macOS
    CRLF,   // Windows
    AUTO    // Detect from file
};

// Code line with line number
struct CodeLine {
    int number;
    std::string text;
};

// Context for displaying a warning
struct WarningContext {
    Warning warning;
    std::vector<CodeLine> lines;
    NolintStyle style;
    int current;  // Current warning number
    int total;    // Total warnings
};

// Modification to apply to a file
struct Modification {
    int line_number;     // Line to modify (1-based)
    NolintStyle style;
    std::string warning_type;
    std::string indent;  // Indentation to match surrounding code
};

} // namespace nolint