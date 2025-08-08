#include "nolint/core/annotated_file.hpp"
#include <algorithm>

namespace nolint {

auto create_annotated_file(const std::vector<std::string>& lines) -> AnnotatedFile {
    AnnotatedFile file;
    file.lines.reserve(lines.size());

    for (const auto& line : lines) {
        file.lines.push_back(
            AnnotatedLine{.text = line, .before_comments = {}, .inline_comment = std::nullopt});
    }

    return file;
}

auto render_annotated_file(const AnnotatedFile& file) -> std::vector<std::string> {
    std::vector<std::string> output;
    output.reserve(file.lines.size() * 2); // Reserve space for annotations

    for (size_t i = 0; i < file.lines.size(); ++i) {
        // 1. CRITICAL: NOLINTBEGIN blocks first (highest priority)
        for (const auto& block : file.blocks) {
            if (block.start_line == i) {
                std::string indent = extract_indentation(file.lines[i].text);
                output.push_back(indent + "// NOLINTBEGIN(" + block.warning_type + ")");
            }
        }

        // 2. CRITICAL: NOLINTNEXTLINE second (must come after NOLINTBEGIN)
        for (const auto& comment : file.lines[i].before_comments) {
            output.push_back(comment);
        }

        // 3. Original line with optional inline comment
        auto line = file.lines[i].text;
        if (file.lines[i].inline_comment) {
            line += "  " + *file.lines[i].inline_comment;
        }
        output.push_back(line);

        // 4. NOLINTEND blocks last
        for (const auto& block : file.blocks) {
            if (block.end_line == i) {
                std::string indent = extract_indentation(file.lines[i].text);
                output.push_back(indent + "// NOLINTEND(" + block.warning_type + ")");
            }
        }
    }

    return output;
}

auto extract_indentation(const std::string& line) -> std::string {
    auto first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return "";
    }
    return line.substr(0, first_non_space);
}

} // namespace nolint