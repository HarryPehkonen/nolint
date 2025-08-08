#include "nolint/parsers/warning_parser.hpp"
#include <optional>
#include <sstream>

namespace nolint {

auto WarningParser::parse_warnings(const std::string& clang_tidy_output) -> std::vector<Warning> {
    std::vector<Warning> warnings;
    std::istringstream iss(clang_tidy_output);
    std::string line;

    std::optional<Warning> current_warning;

    while (std::getline(iss, line)) {
        // Try to parse as warning line
        if (auto warning = parse_single_warning(line)) {
            // Save previous warning if exists
            if (current_warning) {
                warnings.push_back(*current_warning);
            }
            current_warning = *warning;
        }
        // Try to parse as function lines note
        else if (auto function_lines = parse_function_lines_note(line)) {
            if (current_warning) {
                current_warning->function_lines = *function_lines;
            }
        }
    }

    // Don't forget the last warning
    if (current_warning) {
        warnings.push_back(*current_warning);
    }

    return warnings;
}

auto WarningParser::parse_single_warning(const std::string& line) -> std::optional<Warning> {
    std::smatch match;
    if (!std::regex_match(line, match, warning_pattern_)) {
        return std::nullopt;
    }

    try {
        Warning warning{.file_path = match[1].str(),
                        .line_number = static_cast<size_t>(std::stoul(match[2].str())),
                        .column_number = static_cast<size_t>(std::stoul(match[3].str())),
                        .warning_type = match[5].str(),
                        .message = match[4].str(),
                        .function_lines = std::nullopt};
        return warning;
    } catch (const std::exception&) {
        // Invalid number format, skip this warning
        return std::nullopt;
    }
}

auto WarningParser::parse_function_lines_note(const std::string& line) -> std::optional<size_t> {
    std::smatch match;
    if (!std::regex_search(line, match, note_pattern_)) {
        return std::nullopt;
    }

    try {
        return static_cast<size_t>(std::stoul(match[1].str()));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace nolint