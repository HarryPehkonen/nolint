#include "annotated_file.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

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

auto load_annotated_file(const std::string& file_path) -> AnnotatedFile {
    std::vector<std::string> lines;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return create_annotated_file(lines);
}

// Apply inline NOLINT comment
auto apply_inline_suppression(AnnotatedFile file, size_t line_index,
                              const std::string& warning_type) -> AnnotatedFile {
    file.lines[line_index].inline_comment = "// NOLINT(" + warning_type + ")";
    return file;
}

// Apply NOLINTNEXTLINE comment on previous line
auto apply_nextline_suppression(AnnotatedFile file, size_t line_index,
                                const std::string& warning_type) -> AnnotatedFile {
    std::string indent = extract_indentation(file.lines[line_index].text);
    file.lines[line_index].before_comments.push_back(indent + "// NOLINTNEXTLINE(" + warning_type
                                                     + ")");
    return file;
}

// Apply NOLINT block suppression
auto apply_block_suppression(AnnotatedFile file, size_t line_index, const Warning& warning)
    -> AnnotatedFile {
    if (warning.function_lines.has_value()) {
        // Use clang-tidy's function line count directly
        size_t end_line_index = line_index + *warning.function_lines - 1;

        // Ensure we don't go beyond the file
        end_line_index = std::min(end_line_index, file.lines.size() - 1);

        file.blocks.push_back(BlockSuppression{
            .start_line = line_index, .end_line = end_line_index, .warning_type = warning.type});
    } else {
        // Fallback: just wrap the single line if no function_lines info
        file.blocks.push_back(BlockSuppression{
            .start_line = line_index, .end_line = line_index, .warning_type = warning.type});
    }
    return file;
}

auto apply_decision(AnnotatedFile file, const Warning& warning, NolintStyle style)
    -> AnnotatedFile {
    // Validate line number
    if (warning.line_number < 1 || warning.line_number > static_cast<int>(file.lines.size())) {
        return file; // Invalid line number - return unchanged
    }

    size_t line_index = static_cast<size_t>(warning.line_number - 1);

    switch (style) {
    case NolintStyle::NONE:
        // No suppression applied
        break;

    case NolintStyle::NOLINT:
        file = apply_inline_suppression(std::move(file), line_index, warning.type);
        break;

    case NolintStyle::NOLINTNEXTLINE:
        file = apply_nextline_suppression(std::move(file), line_index, warning.type);
        break;

    case NolintStyle::NOLINT_BLOCK:
        file = apply_block_suppression(std::move(file), line_index, warning);
        break;
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

auto save_annotated_file(const AnnotatedFile& file, const std::string& file_path) -> bool {
    std::ofstream output_file(file_path);
    if (!output_file) {
        return false;
    }

    auto rendered_lines = render_annotated_file(file);
    for (const auto& line : rendered_lines) {
        output_file << line << '\n';
    }

    return output_file.good();
}

auto extract_indentation(const std::string& line) -> std::string {
    auto first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return "";
    }
    return line.substr(0, first_non_space);
}

} // namespace nolint
