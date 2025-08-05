#pragma once

#include "nolint_types.hpp"
#include <istream>
#include <string_view>
#include <vector>

namespace nolint {

/**
 * Interface for warning parsers - enables easy testing with mocks
 */
class IWarningParser {
public:
    virtual ~IWarningParser() = default;
    virtual auto parse_warnings(std::istream& input) -> std::vector<Warning> = 0;
    virtual auto parse_warnings(std::string_view input) -> std::vector<Warning> = 0;
};

/**
 * Concrete parser for clang-tidy output
 * Uses regex patterns to extract warning information
 */
class ClangTidyParser : public IWarningParser {
public:
    auto parse_warnings(std::istream& input) -> std::vector<Warning> override;
    auto parse_warnings(std::string_view input) -> std::vector<Warning> override;

private:
    /**
     * Parse a single warning line
     * Returns nullopt if line doesn't match warning pattern
     */
    auto parse_warning_line(std::string_view line) -> std::optional<Warning>;

    /**
     * Parse function size from note line
     * E.g., "note: 44 lines including whitespace and comments"
     * Returns nullopt if not a function size note
     */
    auto parse_function_size_note(std::string_view line) -> std::optional<int>;

    /**
     * Check if this warning type supports NOLINT_BLOCK
     */
    auto supports_block_suppression(std::string_view warning_type) -> bool;
};

namespace functional_core {

/**
 * Pure functions for parsing - easy to test, no I/O
 */

/**
 * Extract warning components from a single clang-tidy warning line
 * Pattern: "/path/file.cpp:123:45: warning: message [warning-type]"
 */
auto extract_warning_components(std::string_view line)
    -> std::optional<std::tuple<std::string, int, int, std::string, std::string>>;

/**
 * Extract function line count from note line
 * Pattern: "note: 44 lines including whitespace and comments"
 */
auto extract_function_lines(std::string_view line) -> std::optional<int>;

/**
 * Check if warning type typically applies to entire functions
 */
constexpr auto is_function_level_warning(std::string_view warning_type) -> bool {
    return warning_type.find("function-size") != std::string_view::npos
           || warning_type.find("function-cognitive-complexity") != std::string_view::npos;
}

/**
 * Normalize file path (handle relative paths, etc.)
 */
auto normalize_file_path(std::string_view path) -> std::string;

} // namespace functional_core

} // namespace nolint