#pragma once

#include "ui_model.hpp"
#include <string>
#include <vector>
#include <optional>
#include <regex>

namespace nolint {

class WarningParser {
public:
    // Parse clang-tidy output into warnings
    auto parse(const std::string& clang_tidy_output) -> std::vector<Warning>;
    
    // Parse from input stream
    auto parse(std::istream& input) -> std::vector<Warning>;

private:
    // Regex pattern for clang-tidy warnings
    // Format: file.cpp:line:col: warning: message [warning-type]
    const std::regex warning_pattern_{
        R"(^([^:]+):(\d+):(\d+):\s+warning:\s+(.+?)\s+\[([^\]]+)\]$)"
    };
    
    // Regex pattern for note lines about function size
    // Format: file.cpp:line:col: note: 35 lines including whitespace and comments...
    const std::regex note_pattern_{
        R"(^[^:]+:\d+:\d+:\s+note:\s+(\d+)\s+lines\s+including.*$)"
    };
    
    // Parse a single line that might be a warning
    auto parse_line(const std::string& line) -> std::optional<Warning>;
};

} // namespace nolint