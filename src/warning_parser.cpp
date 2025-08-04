#include "nolint/warning_parser.hpp"
#include <iostream>
#include <optional>
#include <sstream>

namespace nolint {

// Pattern: /path/to/file.cpp:42:24: warning: message [warning-type]
const std::regex WarningParser::warning_pattern_{R"(^(.+):(\d+):(\d+): warning: (.+) \[(.+)\]$)"};

// Pattern: /path/to/file.cpp:42:24: note: 55 lines including whitespace
const std::regex WarningParser::note_pattern_{R"(^.+: note: (\d+) lines including)"};

auto WarningParser::parse(std::istream& input) -> std::vector<Warning> {
    std::vector<Warning> warnings;
    std::string line;
    Warning* last_warning = nullptr;

    while (std::getline(input, line)) {
        // Try to parse as warning
        if (auto warning = parse_warning_line(line)) {
            warnings.push_back(*warning);
            last_warning = &warnings.back();
        }
        // Try to parse as note (function size)
        else if (auto lines = parse_note_line(line)) {
            if (last_warning
                && last_warning->warning_type.find("function-size") != std::string::npos) {
                last_warning->function_lines = *lines;
            }
        }
    }

    return warnings;
}

auto WarningParser::parse_warning_line(const std::string& line) -> std::optional<Warning> {
    std::smatch match;
    if (!std::regex_match(line, match, warning_pattern_)) {
        return std::nullopt;
    }

    Warning warning;
    warning.file_path = match[1];
    warning.line_number = std::stoi(match[2]);
    warning.column_number = std::stoi(match[3]);
    warning.message = match[4];
    warning.warning_type = match[5];

    return warning;
}

auto WarningParser::parse_note_line(const std::string& line) -> std::optional<int> {
    std::smatch match;
    if (!std::regex_match(line, match, note_pattern_)) {
        return std::nullopt;
    }

    return std::stoi(match[1]);
}

} // namespace nolint