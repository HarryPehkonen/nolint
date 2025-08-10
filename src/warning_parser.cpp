#include "warning_parser.hpp"
#include <sstream>
#include <iostream>

namespace nolint {

auto WarningParser::parse(const std::string& clang_tidy_output) -> std::vector<Warning> {
    std::istringstream stream(clang_tidy_output);
    return parse(stream);
}

auto WarningParser::parse(std::istream& input) -> std::vector<Warning> {
    std::vector<Warning> warnings;
    std::string line;
    
    while (std::getline(input, line)) {
        if (auto warning = parse_line(line)) {
            warnings.push_back(*warning);
        }
    }
    
    return warnings;
}

auto WarningParser::parse_line(const std::string& line) -> std::optional<Warning> {
    std::smatch match;
    
    if (!std::regex_match(line, match, warning_pattern_)) {
        return std::nullopt;
    }
    
    // Extract matched groups
    Warning warning;
    warning.file_path = match[1].str();
    warning.line_number = std::stoi(match[2].str());
    warning.column = std::stoi(match[3].str());
    warning.message = match[4].str();
    warning.type = match[5].str();
    
    return warning;
}

} // namespace nolint
