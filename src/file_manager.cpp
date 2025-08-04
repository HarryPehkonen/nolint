#include "nolint/file_manager.hpp"
#include "nolint/string_utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace nolint {

FileManager::FileManager(std::unique_ptr<IFileSystem> file_system)
    : file_system_(std::move(file_system)) {}

auto FileManager::load_file(const std::string& path) -> void {
    if (files_.find(path) != files_.end()) {
        return; // Already loaded
    }

    auto lines = file_system_->read_file(path);
    FileData data;
    data.lines = lines;
    data.original_lines = lines; // Save for potential rollback
    data.line_ending = detect_line_ending(lines);
    data.modified = false;

    files_[path] = std::move(data);
}

auto FileManager::get_lines(const std::string& path) -> const std::vector<std::string>& {
    load_file(path);
    return files_[path].lines;
}

auto FileManager::apply_modification(const std::string& path, const Modification& mod) -> void {
    load_file(path);
    auto& file_data = files_[path];

    // Adjust line number for any previous insertions
    int actual_line = mod.line_number + file_data.line_offset;

    if (actual_line <= 0 || actual_line > static_cast<int>(file_data.lines.size())) {
        return; // Invalid line number
    }

    switch (mod.style) {
    case NolintStyle::NOLINT_SPECIFIC:
        apply_inline_nolint(file_data, mod);
        break;
    case NolintStyle::NOLINTNEXTLINE:
        apply_nolintnextline(file_data, mod);
        break;
    case NolintStyle::NOLINT_BLOCK:
        apply_nolint_block(file_data, mod);
        break;
    default:
        return; // Unsupported style
    }

    file_data.modified = true;
    total_modifications_++;
}

auto FileManager::write_all() -> void {
    for (auto& [path, data] : files_) {
        if (data.modified) {
            write_file(path);
        }
    }
}

auto FileManager::write_file(const std::string& path) -> void {
    auto it = files_.find(path);
    if (it == files_.end() || !it->second.modified) {
        return;
    }

    auto& file_data = it->second;

    // Apply line endings before writing
    auto lines_to_write = file_data.lines;
    for (auto& line : lines_to_write) {
        apply_line_ending(line, file_data.line_ending);
    }

    file_system_->write_file(path, lines_to_write);
    file_data.modified = false;
}

auto FileManager::rollback_all() -> void {
    for (auto& [path, data] : files_) {
        data.lines = data.original_lines;
        data.modified = false;
    }
    total_modifications_ = 0;
}

auto FileManager::get_modified_file_count() const -> int {
    int count = 0;
    for (const auto& [path, data] : files_) {
        if (data.modified) {
            count++;
        }
    }
    return count;
}

auto FileManager::get_total_modifications() const -> int { return total_modifications_; }

auto FileManager::extract_indentation(const std::string& path, int line_number) -> std::string {
    load_file(path);
    const auto& file_data = files_.at(path);

    if (line_number <= 0 || line_number > static_cast<int>(file_data.lines.size())) {
        return "";
    }

    const auto& line = file_data.lines[line_number - 1]; // Convert to 0-based
    return StringUtils::extract_indentation(line);
}

auto FileManager::find_function_boundaries(const std::string& path, const Warning& warning)
    -> std::pair<int, int> {
    load_file(path);
    const auto& file_data = files_.at(path);

    // For function-size warnings, use the hint from clang-tidy
    if (warning.function_lines) {
        int start_line = find_function_start_line(file_data.lines, warning.line_number);
        int end_line = start_line + *warning.function_lines - 1;
        return {start_line, end_line};
    }

    // For other warnings, create a small block around the warning
    int start_line = std::max(1, warning.line_number - 2);
    int end_line = std::min(static_cast<int>(file_data.lines.size()), warning.line_number + 2);

    return {start_line, end_line};
}

auto FileManager::detect_line_ending(const std::vector<std::string>& lines) -> LineEnding {
    int crlf_count = 0;
    int lf_count = 0;

    for (const auto& line : lines) {
        if (!line.empty()) {
            if (line.back() == '\r') {
                crlf_count++;
            } else {
                lf_count++;
            }
        }
    }

    return (crlf_count > lf_count) ? LineEnding::CRLF : LineEnding::LF;
}

auto FileManager::apply_line_ending(std::string& line, LineEnding ending) -> void {
    // Remove any existing line endings
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }

    // Apply the desired line ending
    if (ending == LineEnding::CRLF) {
        line += "\r\n";
    } else {
        line += "\n";
    }
}

// FileSystem implementation
auto FileSystem::read_file(const std::string& path) -> std::vector<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

auto FileSystem::write_file(const std::string& path, const std::vector<std::string>& lines)
    -> void {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to file: " + path);
    }

    for (const auto& line : lines) {
        file << line;
    }
}

auto FileSystem::file_exists(const std::string& path) -> bool {
    std::ifstream file(path);
    return file.good();
}

auto FileManager::apply_inline_nolint(FileData& file_data, const Modification& mod) -> void {
    int actual_line = mod.line_number + file_data.line_offset;
    auto& line = file_data.lines[actual_line - 1]; // Convert to 0-based
    line += "  // NOLINT(" + mod.warning_type + ")";
}

auto FileManager::apply_nolintnextline(FileData& file_data, const Modification& mod) -> void {
    int actual_line = mod.line_number + file_data.line_offset;
    std::string nolint_line = mod.indent + "// NOLINTNEXTLINE(" + mod.warning_type + ")";
    file_data.lines.insert(file_data.lines.begin() + actual_line - 1, nolint_line);
    file_data.line_offset++; // Track that we added a line
}

auto FileManager::apply_nolint_block(FileData& file_data, const Modification& mod) -> void {
    // Use boundaries from the modification
    int start_line = mod.block_start_line + file_data.line_offset;
    int end_line = mod.block_end_line + file_data.line_offset;

    // Bounds checking
    if (start_line <= 0 || end_line > static_cast<int>(file_data.lines.size())) {
        return; // Invalid boundaries
    }

    // Extract indentation from the start line for consistent formatting
    std::string block_indent = mod.indent;
    if (start_line <= static_cast<int>(file_data.lines.size())) {
        const auto& start_line_text = file_data.lines[start_line - 1];
        auto extracted_indent = StringUtils::extract_indentation(start_line_text);
        if (!extracted_indent.empty()) {
            block_indent = extracted_indent;
        }
    }

    std::string begin_comment = block_indent + "// NOLINTBEGIN(" + mod.warning_type + ")";
    std::string end_comment = block_indent + "// NOLINTEND(" + mod.warning_type + ")";

    // Insert NOLINTBEGIN before the block
    file_data.lines.insert(file_data.lines.begin() + start_line - 1, begin_comment);
    file_data.line_offset++;

    // Insert NOLINTEND after the block (adjust for the line we just added)
    file_data.lines.insert(file_data.lines.begin() + end_line, end_comment);
    file_data.line_offset++;
}

auto FileManager::find_function_start_line(const std::vector<std::string>& lines, int warning_line)
    -> int {
    int start_line = warning_line;

    // Simple heuristic: look for a line that looks like a function signature
    // (contains parentheses and is not indented much)
    for (int i = warning_line - 1; i >= 1; --i) {
        const auto& line = lines[i - 1]; // Convert to 0-based

        // Skip empty lines and comments
        if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }

        // Look for function-like patterns
        if (StringUtils::looks_like_function_signature(line)) {
            // Check if it's not heavily indented (likely a function declaration)
            if (StringUtils::is_lightly_indented(line, 8)) { // Max 8 chars indent
                start_line = i;
                break;
            }
        }
    }

    return start_line;
}

} // namespace nolint