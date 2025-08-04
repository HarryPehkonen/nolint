#pragma once

#include "interfaces.hpp"
#include "types.hpp"
#include <iostream>
#include <iomanip>
#include <termios.h>

namespace nolint {

// Simple console-based user interface
class SimpleUI : public IUserInterface {
public:
    SimpleUI();
    ~SimpleUI();
    
    auto get_user_action() -> UserAction override;
    auto display_context(const WarningContext& context) -> void override;
    auto show_summary(int files_modified, int warnings_suppressed) -> void override;
    auto get_current_style() const -> NolintStyle { return current_style_; }

private:
    FILE* tty_input_;
    NolintStyle current_style_ = NolintStyle::NOLINT_SPECIFIC;
    bool raw_mode_set_ = false;
    struct termios original_termios_;
    
    auto format_nolint_style(NolintStyle style, const std::string& warning_type) -> std::string;
    auto clear_screen() -> void;
    auto highlight_line(const std::string& text) -> std::string;
    auto open_terminal() -> FILE*;
    auto read_key() -> int;  // Read a single key (handles arrow keys)
    auto cycle_style(bool forward) -> void;
    auto setup_raw_mode() -> void;
    auto restore_terminal() -> void;
    auto colorize_comment(const std::string& text) -> std::string;
};

} // namespace nolint