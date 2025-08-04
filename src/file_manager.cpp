#include "nolint/file_manager.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace nolint {

FileManager::FileManager(std::unique_ptr<IFileSystem> file_system)
    : file_system_(std::move(file_system)) {}

auto FileManager::load_file(const std::string& path) -> void {
    if (files_.find(path) != files_.end()) {
        return;  // Already loaded
    }
    
    auto lines = file_system_->read_file(path);
    FileData data;
    data.lines = lines;
    data.original_lines = lines;  // Save for potential rollback
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
        return;  // Invalid line number
    }
    
    switch (mod.style) {
        case NolintStyle::NOLINT_SPECIFIC: {
            // Add inline comment to the end of the line
            auto& line = file_data.lines[actual_line - 1];  // Convert to 0-based
            line += "  // NOLINT(" + mod.warning_type + ")";
            break;
        }
        
        case NolintStyle::NOLINTNEXTLINE: {
            // Insert a new line before the warning line
            std::string nolint_line = mod.indent + "// NOLINTNEXTLINE(" + mod.warning_type + ")";
            file_data.lines.insert(file_data.lines.begin() + actual_line - 1, nolint_line);
            file_data.line_offset++;  // Track that we added a line
            break;
        }
        
        default:
            return;  // Unsupported style
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

auto FileManager::get_total_modifications() const -> int {
    return total_modifications_;
}

auto FileManager::extract_indentation(const std::string& path, int line_number) -> std::string {
    load_file(path);
    const auto& file_data = files_.at(path);
    
    if (line_number <= 0 || line_number > static_cast<int>(file_data.lines.size())) {
        return "";
    }
    
    const auto& line = file_data.lines[line_number - 1];  // Convert to 0-based
    size_t first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return "";
    }
    
    return line.substr(0, first_non_space);
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

auto FileSystem::write_file(const std::string& path, const std::vector<std::string>& lines) -> void {
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

} // namespace nolint