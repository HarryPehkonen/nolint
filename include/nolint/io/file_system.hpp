#pragma once

#include "nolint/core/annotated_file.hpp"
#include "nolint/interfaces.hpp"
#include <string>

namespace nolint {

class FileSystem : public IFileSystem {
public:
    auto read_file_to_annotated(const std::string& path) -> AnnotatedFile override;
    auto write_annotated_file(const AnnotatedFile& file, const std::string& path) -> bool override;
    auto file_exists(const std::string& path) -> bool override;

private:
    auto read_file_lines(const std::string& path) -> std::vector<std::string>;
    auto write_lines_atomic(const std::vector<std::string>& lines, const std::string& path) -> bool;
    auto detect_line_ending(const std::vector<std::string>& lines) -> std::string;
};

} // namespace nolint