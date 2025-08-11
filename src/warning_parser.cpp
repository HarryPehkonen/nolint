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
            
            // Check if a following line (within the next few lines) is a note about function size
            if (!warnings.empty() && warnings.back().type == "readability-function-size") {
                std::streampos pos = input.tellg();
                std::string next_line;
                bool found_note = false;
                
                // Look ahead up to 5 lines for the note (to skip source context lines)
                for (int i = 0; i < 5 && std::getline(input, next_line); ++i) {
                    std::smatch note_match;
                    if (std::regex_match(next_line, note_match, note_pattern_)) {
                        warnings.back().function_lines = std::stoi(note_match[1].str());
                        found_note = true;
                        break;
                    }
                }
                
                if (!found_note) {
                    // Didn't find a note, restore stream position
                    input.seekg(pos);
                }
            }
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
