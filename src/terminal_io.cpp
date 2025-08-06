#include "terminal_io.hpp"

namespace nolint {

// Definition of static members
struct termios* Terminal::s_original_termios = nullptr;
int Terminal::s_tty_fd = -1;

// Static signal handler implementation
void Terminal::restore_terminal_on_signal(int sig) {
    if (s_original_termios && s_tty_fd >= 0) {
        tcsetattr(s_tty_fd, TCSAFLUSH, s_original_termios);
        // Also try to restore stdin/stdout if they were affected
        if (isatty(STDIN_FILENO)) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, s_original_termios);
        }
        if (isatty(STDOUT_FILENO)) {
            tcsetattr(STDOUT_FILENO, TCSAFLUSH, s_original_termios);
        }
    }
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

// Static exit handler implementation
void Terminal::restore_terminal_on_exit() {
    if (s_original_termios && s_tty_fd >= 0) {
        tcsetattr(s_tty_fd, TCSAFLUSH, s_original_termios);
        // Also try to restore stdin/stdout if they were affected
        if (isatty(STDIN_FILENO)) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, s_original_termios);
        }
        if (isatty(STDOUT_FILENO)) {
            tcsetattr(STDOUT_FILENO, TCSAFLUSH, s_original_termios);
        }
    }
}

} // namespace nolint