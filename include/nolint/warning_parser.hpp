#pragma once

#include "interfaces.hpp"
#include "types.hpp"
#include <regex>
#include <string>
#include <optional>

namespace nolint {

class WarningParser : public IWarningParser {
public:
    auto parse(std::istream& input) -> std::vector<Warning> override;

private:
    // Regex patterns for parsing clang-tidy output
    static const std::regex warning_pattern_;
    static const std::regex note_pattern_;
    
    auto parse_warning_line(const std::string& line) -> std::optional<Warning>;
    auto parse_note_line(const std::string& line) -> std::optional<int>;
};

} // namespace nolint