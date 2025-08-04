#include "nolint/simple_ui.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <unistd.h>
#include <termios.h>

namespace nolint {

SimpleUI::SimpleUI() : tty_input_(nullptr), raw_mode_set_(false) {
    tty_input_ = open_terminal();
    if (!tty_input_) {
        throw std::runtime_error("Cannot open terminal for user input");
    }
    setup_raw_mode();
    
    // Debug: Check if raw mode was set
    if (!raw_mode_set_) {
        std::cerr << "Warning: Could not set raw mode for terminal input\n";
    }
}

SimpleUI::~SimpleUI() {
    restore_terminal();
    if (tty_input_ && tty_input_ != stdin) {
        fclose(tty_input_);
    }
}

auto SimpleUI::display_context(const WarningContext& context) -> void {
    clear_screen();
    
    // Show progress
    std::cout << "[" << context.current << "/" << context.total << "] "
              << "Processing " << context.warning.warning_type 
              << " in " << context.warning.file_path 
              << ":" << context.warning.line_number << "\n\n";
    
    // Show warning message
    std::cout << "Warning: " << context.warning.message << "\n";
    if (context.warning.function_lines) {
        std::cout << "Note: " << *context.warning.function_lines << " lines including whitespace\n";
    }
    std::cout << "\n";
    
    // Show code context with potential NOLINTNEXTLINE placement
    for (const auto& line : context.lines) {
        // Show NOLINTNEXTLINE placement
        if (current_style_ == NolintStyle::NOLINTNEXTLINE && 
            line.number == context.warning.line_number - 1) {
            std::cout << "    " << std::setw(4) << line.number << " | " << line.text << "\n";
            
            // Extract indentation from the next line (the warning line)
            std::string indent;
            for (const auto& next_line : context.lines) {
                if (next_line.number == context.warning.line_number) {
                    size_t first_non_space = next_line.text.find_first_not_of(" \t");
                    if (first_non_space != std::string::npos) {
                        indent = next_line.text.substr(0, first_non_space);
                    }
                    break;
                }
            }
            
            std::cout << " +  " << std::setw(4) << " " << " | " 
                     << indent << colorize_comment(format_nolint_style(current_style_, context.warning.warning_type)) << "\n";
        }
        // Show the warning line
        else if (line.number == context.warning.line_number) {
            std::cout << " >> " << std::setw(4) << line.number << " | " 
                     << highlight_line(line.text);
            // Show inline comment for NOLINT styles
            if (current_style_ == NolintStyle::NOLINT || current_style_ == NolintStyle::NOLINT_SPECIFIC) {
                std::cout << "  " << colorize_comment(format_nolint_style(current_style_, context.warning.warning_type));
            }
            std::cout << "\n";
        }
        // Show other context lines
        else {
            std::cout << "    " << std::setw(4) << line.number << " | " 
                     << line.text << "\n";
        }
    }
    
    // Show proposed change
    std::cout << "\nApply NOLINT? Format: " 
              << format_nolint_style(current_style_, context.warning.warning_type) << "\n";
    std::cout << "[Y]es / [N]o / [Q]uit / e[X]it+save / [S]ave file / [↑↓] Change format: ";
    std::cout.flush();
}

auto SimpleUI::get_user_action() -> UserAction {
    while (true) {
        int key = read_key();
        
        switch (key) {
            case 'y':
            case 'Y':
                return UserAction::ACCEPT;
            case 'n':
            case 'N':
            case '\n':
            case '\r':
                return UserAction::SKIP;
            case 'q':
            case 'Q':
                return UserAction::QUIT;
            case 'x':
            case 'X':
                return UserAction::EXIT;
            case 's':
            case 'S':
                return UserAction::SAVE;
            case 27: // ESC sequence (arrow keys)
                // Read the next two characters for arrow keys
                if (read_key() == '[') {
                    int arrow_key = read_key();
                    if (arrow_key == 'A') {  // Up arrow
                        cycle_style(false);  // Previous style
                        return UserAction::STYLE_UP;
                    } else if (arrow_key == 'B') {  // Down arrow
                        cycle_style(true);   // Next style
                        return UserAction::STYLE_DOWN;
                    }
                }
                break;
            default:
                continue;  // Ignore other keys
        }
    }
}

auto SimpleUI::show_summary(int files_modified, int warnings_suppressed) -> void {
    std::cout << "\nSummary:\n";
    std::cout << "✓ " << files_modified << " files modified\n";
    std::cout << "✓ " << warnings_suppressed << " warnings suppressed\n";
}

auto SimpleUI::format_nolint_style(NolintStyle style, const std::string& warning_type) -> std::string {
    switch (style) {
        case NolintStyle::NOLINT:
            return "// NOLINT";
        case NolintStyle::NOLINT_SPECIFIC:
            return "// NOLINT(" + warning_type + ")";
        case NolintStyle::NOLINTNEXTLINE:
            return "// NOLINTNEXTLINE(" + warning_type + ")";
        case NolintStyle::NOLINT_BLOCK:
            return "// NOLINTBEGIN(" + warning_type + ") ... NOLINTEND(" + warning_type + ")";
        default:
            return "// NOLINT(" + warning_type + ")";
    }
}

auto SimpleUI::clear_screen() -> void {
    // Simple clear screen using ANSI escape codes
    std::cout << "\033[2J\033[H";
}

auto SimpleUI::highlight_line(const std::string& text) -> std::string {
    // For MVP, just return the text as-is
    // Future: Add ANSI color codes for highlighting
    return text;
}

auto SimpleUI::open_terminal() -> FILE* {
    // On Unix/Linux, try to open /dev/tty for read/write
    // Need r+ mode to allow tcsetattr to work properly
    FILE* tty = fopen("/dev/tty", "r+");
    if (tty) {
        // Make sure it's unbuffered
        setbuf(tty, nullptr);
        return tty;
    }
    // Fallback to stdin
    return stdin;
}

auto SimpleUI::read_key() -> int {
    return fgetc(tty_input_);
}

auto SimpleUI::cycle_style(bool forward) -> void {
    // Only support two styles for now: NOLINT_SPECIFIC and NOLINTNEXTLINE
    if (forward) {
        if (current_style_ == NolintStyle::NOLINT_SPECIFIC) {
            current_style_ = NolintStyle::NOLINTNEXTLINE;
        } else {
            current_style_ = NolintStyle::NOLINT_SPECIFIC;
        }
    } else {
        if (current_style_ == NolintStyle::NOLINTNEXTLINE) {
            current_style_ = NolintStyle::NOLINT_SPECIFIC;
        } else {
            current_style_ = NolintStyle::NOLINTNEXTLINE;
        }
    }
}

auto SimpleUI::setup_raw_mode() -> void {
    if (!tty_input_) {
        return;
    }
    
    int fd = fileno(tty_input_);
    
    // Get current terminal attributes
    if (tcgetattr(fd, &original_termios_) == -1) {
        return;  // Can't get terminal attributes
    }
    
    struct termios raw = original_termios_;
    
    // Disable canonical mode (line buffering) and echo
    raw.c_lflag &= ~(ECHO | ICANON);
    
    // Set minimum characters to read = 1, no timeout
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    // Apply the new settings immediately
    if (tcsetattr(fd, TCSANOW, &raw) == -1) {
        return;  // Can't set raw mode
    }
    
    raw_mode_set_ = true;
}

auto SimpleUI::restore_terminal() -> void {
    if (raw_mode_set_ && tty_input_) {
        tcsetattr(fileno(tty_input_), TCSAFLUSH, &original_termios_);
        raw_mode_set_ = false;
    }
}

auto SimpleUI::colorize_comment(const std::string& text) -> std::string {
    // ANSI color codes: green for NOLINT comments
    return "\033[32m" + text + "\033[0m";
}

} // namespace nolint