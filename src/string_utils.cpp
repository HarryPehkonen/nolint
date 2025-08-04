#include "nolint/string_utils.hpp"

namespace nolint {

auto StringUtils::extract_indentation(const std::string& line) -> std::string {
    size_t first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return "";
    }
    return line.substr(0, first_non_space);
}

auto StringUtils::looks_like_function_signature(const std::string& line) -> bool {
    return line.find('(') != std::string::npos && line.find(')') != std::string::npos;
}

auto StringUtils::is_lightly_indented(const std::string& line, size_t max_indent) -> bool {
    size_t first_non_space = line.find_first_not_of(" \t");
    return first_non_space == std::string::npos || first_non_space <= max_indent;
}

} // namespace nolint