#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nolint {

// Core data structures using C++20 features
struct Warning {
    std::string file_path;
    int line_number{};
    int column_number{};
    std::string warning_type;
    std::string message;
    std::optional<int> function_lines; // For function-size warnings

    // Equality operator for testing
    bool operator==(const Warning& other) const {
        return file_path == other.file_path && line_number == other.line_number
               && column_number == other.column_number && warning_type == other.warning_type
               && message == other.message && function_lines == other.function_lines;
    }
};

enum class NolintStyle {
    NONE,            // No suppression (allows "undo")
    NOLINT_SPECIFIC, // // NOLINT(warning-type)
    NOLINTNEXTLINE,  // // NOLINTNEXTLINE(warning-type)
    NOLINT_BLOCK     // // NOLINTBEGIN/END(warning-type)
};

struct Modification {
    int target_line{}; // Line where modification applies
    NolintStyle style{};
    std::string warning_type;
    std::string indentation;
    std::optional<int> block_start_line; // For NOLINT_BLOCK
    std::optional<int> block_end_line;   // For NOLINT_BLOCK
};

struct TextTransformation {
    std::vector<std::string> lines;
    int lines_added{};
    int lines_removed{};
};

struct DisplayContext {
    std::vector<std::string> context_lines;
    int warning_line_index{}; // Index in context_lines where warning appears
    std::string format_preview;
    std::string progress_info;
};

// C++20 concepts for type safety
template <typename T>
concept WarningLike = requires(T t) {
    { t.file_path } -> std::convertible_to<std::string_view>;
    { t.line_number } -> std::convertible_to<int>;
    { t.warning_type } -> std::convertible_to<std::string_view>;
};

template <typename T>
concept FileLines = requires(T t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t[0] } -> std::convertible_to<std::string_view>;
};

} // namespace nolint