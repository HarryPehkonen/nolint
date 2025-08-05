# DECISIONS2.md - Second Iteration Decisions

This document captures all the decisions made during the second implementation iteration of the `nolint` tool, including both successful implementations and decisions that were reversed.

## 1. Interactive Mode with Piped Input

### Problem
When running `cat tidy_output.txt | nolint`, the tool couldn't detect user keyboard input because stdin was connected to the pipe, not the terminal. This prevented the interactive arrow key navigation and preview features from working.

### Solution Implemented
**Decision**: Use `/dev/tty` for user interaction while keeping stdin for data input.

**Implementation**:
```cpp
// Terminal class opens /dev/tty when stdin is piped
if (!isatty(STDIN_FILENO)) {
    tty_file_ = fopen("/dev/tty", "r+");
    if (tty_file_) {
        use_tty_ = true;
        setbuf(tty_file_, nullptr); // Disable buffering
    }
}
```

**Result**: Successfully allows interactive mode with piped input by separating data flow (stdin) from user interaction (/dev/tty).

## 2. Terminal Interface Enhancement

### Problem
The original `ITerminal` interface couldn't communicate whether interactive mode was available.

### Solution Implemented
**Decision**: Add `is_interactive()` method to the terminal interface to properly detect interactive capability.

**Implementation**:
```cpp
class ITerminal {
    virtual auto is_interactive() -> bool = 0;
};

// Terminal implementation
auto is_interactive() -> bool override {
    // Interactive if stdin is a terminal OR we have /dev/tty access
    return isatty(STDIN_FILENO) || (use_tty_ && tty_file_);
}
```

**Result**: App can now properly detect when interactive mode is available regardless of stdin status.

## 3. Raw Terminal Mode for Arrow Keys

### Problem
Arrow keys generate escape sequences that need to be captured immediately, not line-buffered.

### Solution Implemented (Initially)
**Decision**: Enable raw terminal mode on `/dev/tty` to capture arrow key sequences.

**Implementation**:
```cpp
// Save original terminal settings and enable raw mode
if (tcgetattr(fileno(tty_file_), &original_termios_) == 0) {
    termios_saved_ = true;
    struct termios raw = original_termios_;
    raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
    raw.c_cc[VMIN] = 1;  // Read at least 1 character
    raw.c_cc[VTIME] = 0; // No timeout
    tcsetattr(fileno(tty_file_), TCSAFLUSH, &raw);
}
```

### Problem Encountered
**Issue**: Terminal state was not properly restored after program exit, requiring user to run `reset` command.

### Solution Attempted (Partially Successful)
**Decision**: Ensure terminal state is restored in destructor and handle signals.

**Implementation**:
```cpp
~Terminal() {
    if (tty_file_) {
        // Restore original terminal settings before closing
        if (termios_saved_) {
            tcsetattr(fileno(tty_file_), TCSAFLUSH, &original_termios_);
            // Also restore stdout terminal settings if they were affected
            if (isatty(STDOUT_FILENO)) {
                tcsetattr(STDOUT_FILENO, TCSAFLUSH, &original_termios_);
            }
        }
        fclose(tty_file_);
    }
}
```

**Status**: Partially resolved - needs signal handler for robust cleanup.

## 4. Arrow Key Detection Implementation

### Solution Implemented
**Decision**: Read character-by-character in raw mode and detect ESC sequences for arrow keys.

**Implementation**:
```cpp
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
            } else if (ch == 127 || ch == 8) { // Backspace
                if (!line.empty()) {
                    line.pop_back();
                }
            }
        }
    } else {
        std::getline(std::cin, line);
    }
    return line;
}
```

**Result**: Successfully captures arrow key sequences (ESC[A for up, ESC[B for down).

## 5. Preview Display Issues

### Problem Identified
The preview system was showing the NOLINT comment format string (e.g., `// NOLINTNEXTLINE(readability-identifier-length)`) instead of showing how the actual code would look with the NOLINT comment applied.

### Current Status
**Issue**: The `build_display_context` function creates a format preview but doesn't show the actual code transformation.

**Code showing the problem**:
```cpp
// In build_display_context:
auto comments = format_nolint_comment(current_style, warning.warning_type);
std::string format_preview = "No suppression";
if (!comments.empty()) {
    format_preview = comments[0];  // This shows "// NOLINTNEXTLINE(...)" 
    if (comments.size() > 1) {
        format_preview += " ... " + comments.back();
    }
}
```

**Expected behavior**: Show the actual source code with NOLINT comments integrated at the correct positions.

**Status**: **UNRESOLVED** - User reported this as not working as expected.

## 6. Interactive Logic Improvements

### Solution Implemented
**Decision**: Use terminal's capability detection rather than manual isatty checks in the application logic.

**Before**:
```cpp
bool can_be_interactive = config_.interactive && (stdin_is_terminal || isatty(STDOUT_FILENO));
```

**After**:
```cpp
bool can_be_interactive = config_.interactive && terminal_->is_interactive();
```

**Result**: Cleaner logic that delegates capability detection to the terminal implementation.

## 7. Error Handling and Fallback

### Solution Implemented
**Decision**: Graceful fallback when `/dev/tty` is not available.

**Implementation**:
```cpp
if (!isatty(STDIN_FILENO) && config_.interactive) {
    if (terminal_->is_interactive()) {
        terminal_->print_line("Input is piped - using /dev/tty for interaction.");
    } else {
        terminal_->print_line("Input is piped and /dev/tty unavailable - using non-interactive mode.");
    }
}
```

**Result**: Tool works in all environments, with clear feedback about mode selection.

## 8. Testing Considerations

### Decision Made
**Decision**: All changes maintain backward compatibility with existing tests.

**Result**: 60/60 tests pass, including:
- 34 functional core tests
- 25 warning parser tests  
- 1 real output integration test (242 warnings parsed)

**Validation**: The implementation doesn't break existing functionality while adding new capabilities.

## Outstanding Issues

### 1. Terminal State Restoration
**Status**: Partially resolved
**Issue**: Terminal requires `reset` command after program exit
**Next Steps**: Add signal handlers for proper cleanup on interruption

### 2. Preview Display
**Status**: Unresolved
**Issue**: Preview shows format string, not actual code transformation
**User Feedback**: "when I hit arrow keys, it cycled through the different ways of applying NOLINT, but it didn't actually show me what it would look like"
**Next Steps**: Modify `build_display_context` to show actual code with NOLINT comments applied

### 3. User Experience Polish
**Status**: Needs improvement
**Issue**: User can't see what they're typing in terminal after program runs
**Root Cause**: Raw terminal mode affects terminal state beyond program execution
**Next Steps**: Implement robust terminal state management with signal handling

## Architecture Decisions Maintained

### 1. Functional Core Separation
**Decision**: Keep pure text transformation functions separate from I/O
**Result**: 34/34 functional core tests pass, validating the separation

### 2. Dependency Injection
**Decision**: Continue using interfaces for all I/O operations
**Result**: Terminal enhancements integrate cleanly with existing mock testing infrastructure

### 3. C++20 Modern Features
**Decision**: Continue leveraging modern C++ features
**Implementation**: Raw terminal mode uses RAII patterns and proper resource management

## Lessons Learned

### 1. Terminal Programming Complexity
**Lesson**: Terminal state management requires careful attention to restoration and signal handling
**Impact**: Need for robust cleanup mechanisms beyond simple destructors

### 2. Preview System Requirements
**Lesson**: Users expect to see actual code transformations, not format descriptions
**Impact**: Need to integrate text transformation functions with display system

### 3. Environment Variability
**Lesson**: `/dev/tty` availability varies across environments
**Impact**: Graceful fallback mechanisms are essential for tool reliability

## Summary

This iteration successfully implemented the core `/dev/tty` solution for interactive mode with piped input, enabling the arrow key navigation that was broken in piped scenarios. The implementation maintains full test compatibility and provides appropriate fallbacks.

However, two significant user experience issues remain:
1. Terminal state restoration needs signal handling
2. Preview system needs to show actual code transformations

The architecture remains sound, with clean separation of concerns and strong test coverage. The next iteration should focus on polishing the user experience while maintaining the robust foundation established.