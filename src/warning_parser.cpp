#include "warning_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace nolint {

auto ClangTidyParser::parse_warnings(std::istream& input) -> std::vector<Warning> {
    std::string content;
    std::string line;

    // Read entire input to string for easier processing
    while (std::getline(input, line)) {
        content += line + "\n";
    }

    return parse_warnings(content);
}

auto ClangTidyParser::parse_warnings(std::string_view input) -> std::vector<Warning> {
    std::vector<Warning> warnings;
    std::istringstream stream{std::string(input)};
    std::string line;

    Warning* current_warning = nullptr;

    while (std::getline(stream, line)) {
        // Try to parse as warning line
        if (auto warning = parse_warning_line(line)) {
            warnings.push_back(std::move(*warning));
            current_warning = &warnings.back();
        }
        // Try to parse as function size note for current warning
        else if (current_warning
                 && current_warning->warning_type.find("function") != std::string::npos) {
            if (auto function_lines = parse_function_size_note(line)) {
                current_warning->function_lines = *function_lines;
            }
        }
    }

    return warnings;
}

auto ClangTidyParser::parse_warning_line(std::string_view line) -> std::optional<Warning> {
    if (auto components = functional_core::extract_warning_components(line)) {
        auto [file_path, line_num, column, message, warning_type] = *components;

        return Warning{.file_path = functional_core::normalize_file_path(file_path),
                       .line_number = line_num,
                       .column_number = column,
                       .warning_type = warning_type,
                       .message = message,
                       .function_lines = std::nullopt};
    }

    return std::nullopt;
}

auto ClangTidyParser::parse_function_size_note(std::string_view line) -> std::optional<int> {
    return functional_core::extract_function_lines(line);
}

auto ClangTidyParser::supports_block_suppression(std::string_view warning_type) -> bool {
    return functional_core::is_function_level_warning(warning_type);
}

namespace functional_core {

auto extract_warning_components(std::string_view line)
    -> std::optional<std::tuple<std::string, int, int, std::string, std::string>> {

    std::string line_str{line};

    // Find the last [...] which should be the warning type
    // Look for ] first, then find the matching [
    auto last_bracket_end = line_str.rfind(']');
    if (last_bracket_end == std::string::npos) {
        return std::nullopt;
    }

    auto last_bracket_start = line_str.rfind('[', last_bracket_end);
    if (last_bracket_start == std::string::npos) {
        return std::nullopt;
    }

    // Extract warning type
    std::string warning_type
        = line_str.substr(last_bracket_start + 1, last_bracket_end - last_bracket_start - 1);

    // Now parse the rest (everything before the final [warning-type])
    std::string prefix = line_str.substr(0, last_bracket_start);

    // Trim trailing whitespace from prefix
    while (!prefix.empty() && std::isspace(prefix.back())) {
        prefix.pop_back();
    }

    // Pattern: "/path/file.cpp:123:45: warning: message"
    static const std::regex prefix_regex{R"(^(.+):(\d+):(\d+):\s*warning:\s*(.+)$)"};

    std::smatch matches;
    if (std::regex_match(prefix, matches, prefix_regex)) {
        try {
            std::string file_path = matches[1].str();
            int line_number = std::stoi(matches[2].str());
            int column_number = std::stoi(matches[3].str());
            std::string message = matches[4].str();

            // Trim trailing whitespace from message
            while (!message.empty() && std::isspace(message.back())) {
                message.pop_back();
            }

            return std::make_tuple(std::move(file_path), line_number, column_number,
                                   std::move(message), std::move(warning_type));
        } catch (const std::exception&) {
            // Invalid numbers in line/column
            return std::nullopt;
        }
    }

    return std::nullopt;
}

auto extract_function_lines(std::string_view line) -> std::optional<int> {
    // Pattern matches both full paths and bare notes:
    // "/path/file.cpp:78:10: note: 44 lines including whitespace and comments (threshold 30)"
    // "note: 25 lines including whitespace"
    // "  note: 100 lines including comments"
    static const std::regex note_regex{R"(^.*note:\s*(\d+)\s+lines?\s+.*$)"};

    std::string line_str{line};
    std::smatch matches;

    if (std::regex_match(line_str, matches, note_regex)) {
        try {
            return std::stoi(matches[1].str());
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

auto normalize_file_path(std::string_view path) -> std::string {
    std::string result{path};

    // Remove leading "./" if present
    if (result.starts_with("./")) {
        result = result.substr(2);
    }
    // Keep all absolute paths as-is - they're already correct and useful

    // Could add more normalization logic here
    // - Resolve relative paths
    // - Convert to absolute paths
    // - Handle different path separators

    return result;
}

} // namespace functional_core

} // namespace nolint