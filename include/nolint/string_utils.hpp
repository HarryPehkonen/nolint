#pragma once

#include <string>

namespace nolint {

class StringUtils {
public:
    // Extract indentation (leading whitespace) from a line
    static auto extract_indentation(const std::string& line) -> std::string;

    // Check if a line looks like a function signature (has parentheses)
    static auto looks_like_function_signature(const std::string& line) -> bool;

    // Check if a line is lightly indented (likely a function declaration)
    static auto is_lightly_indented(const std::string& line, size_t max_indent = 8) -> bool;
};

} // namespace nolint