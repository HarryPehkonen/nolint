#pragma once

#include "nolint/core/warning.hpp"
#include "nolint/interfaces.hpp"
#include <regex>
#include <string>
#include <vector>

namespace nolint {

class WarningParser : public IWarningParser {
public:
    auto parse_warnings(const std::string& clang_tidy_output) -> std::vector<Warning> override;

private:
    auto parse_single_warning(const std::string& line) -> std::optional<Warning>;
    auto parse_function_lines_note(const std::string& line) -> std::optional<size_t>;
    
    // Regex patterns for clang-tidy output
    static inline const std::regex warning_pattern_{
        R"(^(.+):(\d+):(\d+):\s+warning:\s+(.+)\s+\[(.+)\]$)"};
    static inline const std::regex note_pattern_{
        R"(note:\s+(\d+)\s+lines)"};
};

} // namespace nolint