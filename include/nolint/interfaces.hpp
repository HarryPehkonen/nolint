#pragma once

#include <memory>
#include <string>
#include <vector>

namespace nolint {

// Forward declarations
struct Warning;
struct AnnotatedFile;
struct UIModel;
struct Screen;
enum class InputEvent;

// Abstract interfaces for dependency injection
class ITerminal {
public:
    virtual ~ITerminal() = default;
    virtual auto setup_raw_mode() -> bool = 0;
    virtual auto get_input_event() -> InputEvent = 0;
    virtual auto display_screen(const Screen& screen) -> void = 0;
    virtual auto read_line() -> std::string = 0;
    virtual auto is_interactive() -> bool = 0;
    virtual auto restore_terminal_state() -> void = 0;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual auto read_file_to_annotated(const std::string& path) -> AnnotatedFile = 0;
    virtual auto write_annotated_file(const AnnotatedFile& file, const std::string& path) -> bool = 0;
    virtual auto file_exists(const std::string& path) -> bool = 0;
};

class IWarningParser {
public:
    virtual ~IWarningParser() = default;
    virtual auto parse_warnings(const std::string& clang_tidy_output) -> std::vector<Warning> = 0;
};

} // namespace nolint