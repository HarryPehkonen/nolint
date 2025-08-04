#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace nolint {

// Forward declarations
struct Warning;
struct WarningContext;
enum class UserAction;

// Interface for parsing clang-tidy output
class IWarningParser {
public:
    virtual ~IWarningParser() = default;
    virtual auto parse(std::istream& input) -> std::vector<Warning> = 0;
};

// Interface for file system operations
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual auto read_file(const std::string& path) -> std::vector<std::string> = 0;
    virtual auto write_file(const std::string& path, const std::vector<std::string>& lines) -> void = 0;
    virtual auto file_exists(const std::string& path) -> bool = 0;
};

// Interface for user interaction
class IUserInterface {
public:
    virtual ~IUserInterface() = default;
    virtual auto get_user_action() -> UserAction = 0;
    virtual auto display_context(const WarningContext& context) -> void = 0;
    virtual auto show_summary(int files_modified, int warnings_suppressed) -> void = 0;
};

} // namespace nolint