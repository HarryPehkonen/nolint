#pragma once

#include <optional>
#include <string>

namespace nolint {

struct Warning {
    std::string file_path;
    size_t line_number{};
    size_t column_number{};
    std::string warning_type;
    std::string message;
    std::optional<size_t> function_lines;  // Only for function-size warnings

    auto operator==(const Warning& other) const -> bool = default;
};

// NOLINT suppression styles
enum class NolintStyle {
    NONE,              // No suppression (allows "undo")
    NOLINT_SPECIFIC,   // // NOLINT(warning-type)
    NOLINTNEXTLINE,    // // NOLINTNEXTLINE(warning-type)  
    NOLINT_BLOCK       // // NOLINTBEGIN/END(warning-type)
};

// Generate unique key for warning identification
auto warning_key(const Warning& warning) -> std::string;

// Get next style in cycle for given warning
auto cycle_style_up(NolintStyle current, const Warning& warning) -> NolintStyle;
auto cycle_style_down(NolintStyle current, const Warning& warning) -> NolintStyle;

// Check if style is available for warning type
auto is_style_available(NolintStyle style, const Warning& warning) -> bool;

// Get style display name
auto style_display_name(NolintStyle style) -> std::string;

} // namespace nolint