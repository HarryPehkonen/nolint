#pragma once

#include "nolint/interfaces.hpp"
#include "nolint/ui/ui_model.hpp"
#include <cstdio>
#include <termios.h>

namespace nolint {

class Terminal : public ITerminal {
private:
    FILE* tty_file_ = nullptr;          // /dev/tty for piped input support
    bool use_tty_ = false;              // Flag for /dev/tty usage
    bool termios_saved_ = false;        // RAII state tracking
    struct termios original_termios_{};  // For restoration
    
    // Static state for signal handling (CRITICAL for cleanup)
    static struct termios* s_original_termios_;
    static int s_tty_fd_;
    static void restore_terminal_on_signal(int sig);
    
public:
    Terminal();
    ~Terminal() override;
    
    // Delete copy operations to prevent double cleanup
    Terminal(const Terminal&) = delete;
    auto operator=(const Terminal&) -> Terminal& = delete;
    
    // ITerminal interface
    auto setup_raw_mode() -> bool override;
    auto get_input_event() -> InputEvent override;
    auto display_screen(const Screen& screen) -> void override;
    auto read_line() -> std::string override;
    auto is_interactive() -> bool override;
    auto restore_terminal_state() -> void override;
    
private:
    auto setup_signal_handlers() -> void;
    auto clear_screen() -> void;
    auto read_char() -> char;
    auto read_arrow_sequence() -> InputEvent;
};

} // namespace nolint