#pragma once

#include <csignal> // for signal handling
#include <iostream>
#include <sstream>
#include <string>
#include <termios.h> // for terminal control
#include <unistd.h>  // for isatty

namespace nolint {

// Forward declaration for Terminal class
class Terminal;

// Abstract interface for terminal I/O operations (for testability)
class ITerminal {
public:
    virtual ~ITerminal() = default;

    virtual auto print(const std::string& message) -> void = 0;
    virtual auto print_line(const std::string& message) -> void = 0;
    virtual auto read_line() -> std::string = 0;
    virtual auto read_char() -> char = 0;
    virtual auto is_interactive() -> bool = 0;
};

// Concrete implementation using actual terminal
class Terminal : public ITerminal {
private:
    FILE* tty_file_ = nullptr;
    bool use_tty_ = false;
    struct termios original_termios_;
    bool termios_saved_ = false;

    // Static members for signal handling (better encapsulation than globals)
    static struct termios* s_original_termios;
    static int s_tty_fd;

    // Static signal handler methods
    static void restore_terminal_on_signal(int sig);
    static void restore_terminal_on_exit();

public:
    Terminal() {
        // If stdin is not a terminal (piped input), try to open /dev/tty for interaction
        if (!isatty(STDIN_FILENO)) {
            tty_file_ = fopen("/dev/tty", "r+");
            if (tty_file_) {
                use_tty_ = true;
                setbuf(tty_file_, nullptr); // Disable buffering

                // Save original terminal settings and enable raw mode for arrow keys
                if (tcgetattr(fileno(tty_file_), &original_termios_) == 0) {
                    termios_saved_ = true;

                    // Set up static signal handling for terminal restoration
                    s_original_termios = &original_termios_;
                    s_tty_fd = fileno(tty_file_);

                    // Install signal handlers and exit handler
                    std::signal(SIGINT, restore_terminal_on_signal);
                    std::signal(SIGTERM, restore_terminal_on_signal);
                    std::signal(SIGHUP, restore_terminal_on_signal);
                    std::atexit(restore_terminal_on_exit);

                    struct termios raw = original_termios_;
                    raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
                    raw.c_cc[VMIN] = 1;              // Read at least 1 character
                    raw.c_cc[VTIME] = 0;             // No timeout
                    tcsetattr(fileno(tty_file_), TCSAFLUSH, &raw);
                }
            }
        }
    }

    ~Terminal() {
        if (tty_file_) {
            // Restore original terminal settings before closing
            if (termios_saved_) {
                tcsetattr(fileno(tty_file_), TCSAFLUSH, &original_termios_);
                // Also restore stdout terminal settings if they were affected
                if (isatty(STDOUT_FILENO)) {
                    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &original_termios_);
                }

                // Clear static state
                s_original_termios = nullptr;
                s_tty_fd = -1;

                // Restore default signal handlers
                std::signal(SIGINT, SIG_DFL);
                std::signal(SIGTERM, SIG_DFL);
                std::signal(SIGHUP, SIG_DFL);
            }
            fclose(tty_file_);
        }
    }

    auto print(const std::string& message) -> void override { std::cout << message << std::flush; }

    auto print_line(const std::string& message) -> void override { std::cout << message << '\n'; }

    auto read_line() -> std::string override {
        std::string line;
        if (use_tty_ && tty_file_) {
            // In raw mode, read character by character until enter or arrow key
            char ch;
            while ((ch = fgetc(tty_file_)) != EOF) {
                if (ch == '\n' || ch == '\r') {
                    break;
                } else if (ch == 27) { // ESC sequence (arrow keys)
                    line += ch;
                    // Read the rest of the arrow key sequence
                    if ((ch = fgetc(tty_file_)) != EOF) {
                        line += ch;
                        if (ch == '[') {
                            if ((ch = fgetc(tty_file_)) != EOF) {
                                line += ch;
                            }
                        }
                    }
                    break;
                } else if (ch >= 32 && ch <= 126) { // Printable characters
                    line += ch;
                    // Echo the character since we disabled terminal echo
                    std::cout << ch << std::flush;
                } else if (ch == 127 || ch == 8) { // Backspace
                    if (!line.empty()) {
                        line.pop_back();
                        // Echo backspace: move cursor back, print space, move cursor back again
                        std::cout << "\b \b" << std::flush;
                    }
                }
            }
            // Print newline after user presses enter
            std::cout << '\n' << std::flush;
        } else {
            std::getline(std::cin, line);
        }
        return line;
    }

    auto read_char() -> char override {
        if (use_tty_ && tty_file_) {
            return fgetc(tty_file_);
        } else {
            char c;
            std::cin >> c;
            std::cin.ignore(); // Ignore the newline after the character
            return c;
        }
    }

    auto is_interactive() -> bool override {
        // Interactive if stdin is a terminal OR we have /dev/tty access
        return isatty(STDIN_FILENO) || (use_tty_ && tty_file_);
    }
};

// Mock implementation for testing
class MockTerminal : public ITerminal {
private:
    std::ostringstream output_;
    std::istringstream input_;

public:
    explicit MockTerminal(const std::string& input = "") : input_(input) {}

    auto print(const std::string& message) -> void override { output_ << message; }

    auto print_line(const std::string& message) -> void override { output_ << message << '\n'; }

    auto read_line() -> std::string override {
        std::string line;
        std::getline(input_, line);
        return line;
    }

    auto read_char() -> char override {
        char c;
        input_ >> c;
        return c;
    }

    auto get_output() const -> std::string { return output_.str(); }

    auto reset_input(const std::string& input) -> void {
        input_.str(input);
        input_.clear();
    }

    auto is_interactive() -> bool override {
        return true; // Mock is always interactive for testing
    }
};

} // namespace nolint