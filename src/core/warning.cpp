#include "nolint/core/warning.hpp"
#include <sstream>

namespace nolint {

auto warning_key(const Warning& warning) -> std::string {
    std::ostringstream oss;
    oss << warning.file_path << ":" << warning.line_number << ":" << warning.column_number;
    return oss.str();
}

auto cycle_style_up(NolintStyle current, const Warning& warning) -> NolintStyle {
    switch (current) {
    case NolintStyle::NONE:
        return NolintStyle::NOLINT_SPECIFIC;
    case NolintStyle::NOLINT_SPECIFIC:
        return NolintStyle::NOLINTNEXTLINE;
    case NolintStyle::NOLINTNEXTLINE:
        if (is_style_available(NolintStyle::NOLINT_BLOCK, warning)) {
            return NolintStyle::NOLINT_BLOCK;
        }
        return NolintStyle::NONE;
    case NolintStyle::NOLINT_BLOCK:
        return NolintStyle::NONE;
    }
    return NolintStyle::NONE;
}

auto cycle_style_down(NolintStyle current, const Warning& warning) -> NolintStyle {
    switch (current) {
    case NolintStyle::NONE:
        if (is_style_available(NolintStyle::NOLINT_BLOCK, warning)) {
            return NolintStyle::NOLINT_BLOCK;
        }
        return NolintStyle::NOLINTNEXTLINE;
    case NolintStyle::NOLINT_SPECIFIC:
        return NolintStyle::NONE;
    case NolintStyle::NOLINTNEXTLINE:
        return NolintStyle::NOLINT_SPECIFIC;
    case NolintStyle::NOLINT_BLOCK:
        return NolintStyle::NOLINTNEXTLINE;
    }
    return NolintStyle::NONE;
}

auto is_style_available(NolintStyle style, const Warning& warning) -> bool {
    switch (style) {
    case NolintStyle::NONE:
    case NolintStyle::NOLINT_SPECIFIC:
    case NolintStyle::NOLINTNEXTLINE:
        return true;
    case NolintStyle::NOLINT_BLOCK:
        // Only available for function-size warnings
        return warning.function_lines.has_value();
    }
    return false;
}

auto style_display_name(NolintStyle style) -> std::string {
    switch (style) {
    case NolintStyle::NONE:
        return "No suppression";
    case NolintStyle::NOLINT_SPECIFIC:
        return "// NOLINT(warning-type)";
    case NolintStyle::NOLINTNEXTLINE:
        return "// NOLINTNEXTLINE(warning-type)";
    case NolintStyle::NOLINT_BLOCK:
        return "// NOLINTBEGIN(warning-type) ... // NOLINTEND(warning-type)";
    }
    return "Unknown";
}

} // namespace nolint