#pragma once

#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace nolint {

// Abstract interface for file I/O operations (for testability)
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual auto read_file(const std::string& path) -> std::vector<std::string> = 0;
    virtual auto write_file(const std::string& path, std::span<const std::string> lines) -> bool
        = 0;
    virtual auto file_exists(const std::string& path) -> bool = 0;
};

// Concrete implementation using actual filesystem
class FileSystem : public IFileSystem {
public:
    auto read_file(const std::string& path) -> std::vector<std::string> override {
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

    auto write_file(const std::string& path, std::span<const std::string> lines) -> bool override {
        std::ofstream file(path);

        if (!file.is_open()) {
            return false;
        }

        for (const auto& line : lines) {
            file << line << '\n';
        }

        return file.good();
    }

    auto file_exists(const std::string& path) -> bool override {
        std::ifstream file(path);
        return file.good();
    }
};

} // namespace nolint