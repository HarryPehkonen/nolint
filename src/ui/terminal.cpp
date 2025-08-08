#include "nolint/ui/terminal.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/select.h>
#include <unistd.h>

namespace nolint {

// Static members for signal handling
struct termios* Terminal::s_original_termios_ = nullptr;
int Terminal::s_tty_fd_ = -1;

Terminal::Terminal() {
    // Try to open /dev/tty for piped input support
    if (!isatty(STDIN_FILENO)) {
        tty_file_ = fopen("/dev/tty", "r+");
        if (tty_file_) {
            setbuf(tty_file_, nullptr); // Disable buffering
            use_tty_ = true;
        }
    }
}

Terminal::~Terminal() { restore_terminal_state(); }

auto Terminal::setup_raw_mode() -> bool {
    FILE* terminal_file = use_tty_ ? tty_file_ : stdin;
    if (!terminal_file) {
        return false;
    }

    int fd = fileno(terminal_file);
    if (tcgetattr(fd, &original_termios_) != 0) {
        return false;
    }

    termios_saved_ = true;

    // Setup static state for signal handler
    s_original_termios_ = &original_termios_;
    s_tty_fd_ = fd;

    // Register signal handlers for terminal restoration
    setup_signal_handlers();

    // Register atexit handler as final safety net
    std::atexit([]() {
        if (s_original_termios_ && s_tty_fd_ >= 0) {
            tcsetattr(s_tty_fd_, TCSAFLUSH, s_original_termios_);
        }
    });

    // Configure raw mode
    struct termios raw = original_termios_;
    raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
    raw.c_cc[VMIN] = 1;              // Read at least 1 character
    raw.c_cc[VTIME] = 0;             // No timeout

    if (tcsetattr(fd, TCSAFLUSH, &raw) != 0) {
        termios_saved_ = false;
        return false;
    }

    return true;
}

auto Terminal::get_input_event() -> InputEvent {
    char ch = read_char();

    // Handle arrow key sequences (ESC [ A/B/C/D)
    if (ch == 27) { // ESC
        return read_arrow_sequence();
    }

    // Single character commands
    switch (ch) {
    case 'x':
    case 'X':
        return InputEvent::SAVE_EXIT;
    case 'q':
    case 'Q':
        return InputEvent::QUIT;
    case '/':
        return InputEvent::SEARCH;
    case 't':
    case 'T':
        return InputEvent::SHOW_STATISTICS;
    case '\r':
    case '\n':
        return InputEvent::ENTER;
    default:
        return InputEvent::UNKNOWN;
    }
}

auto Terminal::display_screen(const Screen& screen) -> void {
    clear_screen();

    for (const auto& line : screen.content) {
        if (line.is_highlighted) {
            std::cout << "\033[32m"; // Green
        }
        std::cout << line.text;
        if (line.is_highlighted) {
            std::cout << "\033[0m"; // Reset
        }
        std::cout << '\n';
    }

    std::cout << "\n" << screen.status_line << "\n";
    std::cout << screen.control_hints << "\n> ";
    std::cout.flush();
}

auto Terminal::read_line() -> std::string {
    std::string line;
    FILE* input_file = use_tty_ ? tty_file_ : stdin;

    if (use_tty_) {
        // Manual character echoing in raw mode for search visibility
        char ch;
        while ((ch = fgetc(input_file)) != EOF) {
            if (ch == '\n' || ch == '\r') {
                std::cout << '\n';
                break;
            } else if (ch >= 32 && ch <= 126) { // Printable characters
                line += ch;
                std::cout << ch << std::flush; // Manual echo
            } else if (ch == 127 || ch == 8) { // Backspace/Delete
                if (!line.empty()) {
                    line.pop_back();
                    std::cout << "\b \b" << std::flush; // Erase character
                }
            }
        }
    } else {
        std::getline(std::cin, line);
    }

    return line;
}

auto Terminal::is_interactive() -> bool { return isatty(STDIN_FILENO) || (use_tty_ && tty_file_); }

auto Terminal::restore_terminal_state() -> void {
    if (termios_saved_) {
        FILE* terminal_file = use_tty_ ? tty_file_ : stdin;
        if (terminal_file) {
            tcsetattr(fileno(terminal_file), TCSAFLUSH, &original_termios_);
        }
        termios_saved_ = false;
    }

    if (tty_file_) {
        fclose(tty_file_);
        tty_file_ = nullptr;
        use_tty_ = false;
    }

    // Clear static state
    s_original_termios_ = nullptr;
    s_tty_fd_ = -1;
}

void Terminal::restore_terminal_on_signal(int sig) {
    if (s_original_termios_ && s_tty_fd_ >= 0) {
        tcsetattr(s_tty_fd_, TCSAFLUSH, s_original_termios_);
    }
    std::signal(sig, SIG_DFL); // Restore default handler
    std::raise(sig);           // Re-raise signal
}

auto Terminal::setup_signal_handlers() -> void {
    std::signal(SIGINT, restore_terminal_on_signal);  // Ctrl+C
    std::signal(SIGTERM, restore_terminal_on_signal); // Termination
    std::signal(SIGQUIT, restore_terminal_on_signal); // Ctrl+backslash
    std::signal(SIGTSTP, restore_terminal_on_signal); // Ctrl+Z
}

auto Terminal::clear_screen() -> void {
    std::cout << "\033[2J\033[H"; // Clear screen and move cursor to top
}

auto Terminal::read_char() -> char {
    FILE* input_file = use_tty_ ? tty_file_ : stdin;
    return static_cast<char>(fgetc(input_file));
}

auto Terminal::read_arrow_sequence() -> InputEvent {
    FILE* input_file = use_tty_ ? tty_file_ : stdin;
    int fd = fileno(input_file);
    
    // Set a very short timeout to distinguish between ESC and arrow sequences
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms timeout
    
    int result = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    
    if (result <= 0) {
        // Timeout or error - treat as standalone ESC
        return InputEvent::ESCAPE;
    }
    
    // Data is available, read it
    char next = read_char();
    
    if (next == '[') {
        char arrow = read_char();
        switch (arrow) {
        case 'A':
            return InputEvent::ARROW_UP;
        case 'B':
            return InputEvent::ARROW_DOWN;
        case 'C':
            return InputEvent::ARROW_RIGHT;
        case 'D':
            return InputEvent::ARROW_LEFT;
        }
    }
    
    // Not a valid arrow sequence, treat as ESC
    return InputEvent::ESCAPE;
}

} // namespace nolint