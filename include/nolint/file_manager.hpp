#pragma once

#include "interfaces.hpp"
#include "types.hpp"
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nolint {

// Manages file content and modifications in memory
class FileManager {
public:
    struct FileData {
        std::vector<std::string> lines;
        std::vector<std::string> original_lines; // For rollback
        LineEnding line_ending = LineEnding::LF;
        int line_offset = 0; // Track how many lines have been added
        bool modified = false;
    };

    explicit FileManager(std::unique_ptr<IFileSystem> file_system);

    // Load file if not already loaded
    auto load_file(const std::string& path) -> void;

    // Get lines for a file (loads if necessary)
    auto get_lines(const std::string& path) -> const std::vector<std::string>&;

    // Apply a modification to a file
    auto apply_modification(const std::string& path, const Modification& mod) -> void;

    // Write all pending changes
    auto write_all() -> void;

    // Write a specific file
    auto write_file(const std::string& path) -> void;

    // Rollback all changes
    auto rollback_all() -> void;

    // Get modification statistics
    auto get_modified_file_count() const -> int;
    auto get_total_modifications() const -> int;

    // Utility method to extract indentation from a line
    auto extract_indentation(const std::string& path, int line_number) -> std::string;

    // Find function boundaries for block suppressions
    auto find_function_boundaries(const std::string& path, const Warning& warning)
        -> std::pair<int, int>;

    // Helper method to find function start line by searching backwards
    auto find_function_start_line(const std::vector<std::string>& lines, int warning_line) -> int;

private:
    std::unique_ptr<IFileSystem> file_system_;
    std::map<std::string, FileData> files_;
    int total_modifications_ = 0;

    auto detect_line_ending(const std::vector<std::string>& lines) -> LineEnding;
    auto apply_line_ending(std::string& line, LineEnding ending) -> void;

    // Modification application methods
    auto apply_inline_nolint(FileData& file_data, const Modification& mod) -> void;
    auto apply_nolintnextline(FileData& file_data, const Modification& mod) -> void;
    auto apply_nolint_block(FileData& file_data, const Modification& mod) -> void;
};

// RAII wrapper for file modifications
class FileTransaction {
    FileManager& manager_;
    bool committed_ = false;

public:
    explicit FileTransaction(FileManager& manager) : manager_(manager) {}

    ~FileTransaction() {
        if (!committed_) {
            manager_.rollback_all();
        }
    }

    auto commit() -> void {
        manager_.write_all();
        committed_ = true;
    }

    // Delete copy operations
    FileTransaction(const FileTransaction&) = delete;
    auto operator=(const FileTransaction&) -> FileTransaction& = delete;

    // Move operations
    FileTransaction(FileTransaction&& other) noexcept
        : manager_(other.manager_), committed_(other.committed_) {
        other.committed_ = true; // Prevent rollback in moved-from object
    }
};

// Concrete file system implementation
class FileSystem : public IFileSystem {
public:
    auto read_file(const std::string& path) -> std::vector<std::string> override;
    auto write_file(const std::string& path, const std::vector<std::string>& lines)
        -> void override;
    auto file_exists(const std::string& path) -> bool override;
};

} // namespace nolint