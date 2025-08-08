#include "nolint/io/file_system.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nolint {

auto FileSystem::read_file_to_annotated(const std::string& path) -> AnnotatedFile {
    auto lines = read_file_lines(path);
    return create_annotated_file(lines);
}

auto FileSystem::write_annotated_file(const AnnotatedFile& file, const std::string& path) -> bool {
    auto rendered_lines = render_annotated_file(file);
    return write_lines_atomic(rendered_lines, path);
}

auto FileSystem::file_exists(const std::string& path) -> bool {
    return std::filesystem::exists(path);
}

auto FileSystem::read_file_lines(const std::string& path) -> std::vector<std::string> {
    std::vector<std::string> lines;
    std::ifstream file(path);

    if (!file.is_open()) {
        return lines; // Return empty vector on error
    }

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

auto FileSystem::write_lines_atomic(const std::vector<std::string>& lines, const std::string& path)
    -> bool {
    try {
        // Write to temporary file first for atomic operation
        std::string temp_path = path + ".tmp";

        {
            std::ofstream file(temp_path);
            if (!file.is_open()) {
                return false;
            }

            std::string line_ending = detect_line_ending(lines);

            for (const auto& line : lines) {
                file << line << line_ending;
            }

            if (file.fail()) {
                return false;
            }
        } // File automatically closed here

        // Atomically replace original file
        std::filesystem::rename(temp_path, path);
        return true;

    } catch (const std::exception&) {
        // Clean up temporary file on error
        try {
            std::filesystem::remove(path + ".tmp");
        } catch (...) {
            // Ignore cleanup errors
        }
        return false;
    }
}

auto FileSystem::detect_line_ending([[maybe_unused]] const std::vector<std::string>& lines)
    -> std::string {
    // Simple line ending detection - default to LF on Unix systems
    // TODO: Could be enhanced to detect from original file
    return "\n";
}

} // namespace nolint