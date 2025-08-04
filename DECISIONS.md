# NOLINT - Design Decisions and Learnings

## Overview

This document captures the design decisions, implementation approaches, and lessons learned during the development of `nolint`. It includes both successful approaches and failed attempts.

## Major Design Decisions

### 1. Architecture: File-based vs Direct Execution

**Decision**: Support file-based input primarily (`--input file.txt`), with pipe support

**Rationale**:
- Robustness: Large projects generate extensive output easier to handle from file
- Flexibility: Users can pre-generate reports and run nolint multiple times
- Resumability: Can potentially continue where left off if interrupted
- Debugging: Easier to inspect clang-tidy output

**What We Considered**: 
- Running clang-tidy directly from the tool (`--run-clang-tidy`)
- We decided against this for the MVP to keep it simple and focused

### 2. In-Memory File Transformation

**Decision**: Load entire files into `std::vector<std::string>` and apply all changes in memory before writing

**Rationale**:
- Line number consistency during processing
- Easy rollback capability
- Handles multiple warnings per file efficiently
- Atomic writes prevent partial modifications

**What Worked**: This approach made line offset tracking much simpler than trying to write incrementally

### 3. Terminal Input with Piped Data

**Problem**: When piping clang-tidy output to nolint, `std::cin` is connected to the pipe, not the keyboard

**Initial Attempt**: Use `std::getline(std::cin, input)` - FAILED
- When stdin is piped, can't read user input
- Tool would immediately exit reading empty/EOF from pipe

**Solution**: Open `/dev/tty` directly for user input
```cpp
FILE* tty = fopen("/dev/tty", "r+");
```

**Learning**: Always separate data input (stdin) from user interaction (terminal) in pipe-friendly tools

### 4. Single-Key Input (No Enter Required)

**Problem**: Users had to press Enter after each key, making navigation cumbersome

**Initial Attempts**:
1. `fgets()` with `/dev/tty` - FAILED (still line-buffered)
2. Raw mode on stdin - FAILED (wrong file descriptor)
3. Raw mode with `fopen("/dev/tty", "r")` - FAILED (read-only mode)

**Solution**: 
- Open `/dev/tty` in read/write mode (`"r+"`)
- Set terminal to raw mode using `tcsetattr()`
- Disable line buffering and echo
- Use `TCSANOW` instead of `TCSAFLUSH` for immediate effect
- Call `setbuf(tty, nullptr)` to ensure no buffering

**Code that worked**:
```cpp
FILE* tty = fopen("/dev/tty", "r+");
setbuf(tty, nullptr);
// ... then set raw mode with termios
```

### 5. RAII for Terminal State

**Decision**: Use constructor/destructor to manage terminal raw mode

**Rationale**: 
- Ensures terminal is always restored, even on exceptions
- Clean, automatic resource management
- Follows C++ best practices

### 6. MVP Feature Set

**What We Included**:
- Basic `// NOLINT(warning-type)` inline comments
- `// NOLINTNEXTLINE(warning-type)` support with arrow key navigation
- Y/N/Q/X/S single-key commands
- ANSI color codes (green) for NOLINT comments
- Progress tracking

**What We Deferred**:
- `// NOLINTBEGIN/END` block comments
- Multiple format cycling beyond two styles
- Windows support
- Configuration files
- Backup files
- Arrow key style selection in initial MVP (added after core features worked)

### 7. Testability Through Dependency Injection

**Decision**: Use interfaces for all major components

**Benefits**:
- Unit testing without file I/O
- Mock implementations for testing
- Clean separation of concerns
- Easy to extend/modify behavior

### 8. Line Ending Handling

**Decision**: Auto-detect and preserve original line endings

**Implementation**: Count CRLF vs LF occurrences to determine file's convention

### 9. Indentation Matching

**Decision**: Extract indentation from warning line for NOLINTNEXTLINE placement

**Implementation**: Find first non-whitespace character, preserve everything before it

### 10. Style Selection UI

**Initial Design**: Fixed style selection
**Enhancement**: Arrow keys to cycle through styles with live preview
**What Worked**: Showing the actual comment placement before user commits

## Platform Decisions

### Windows Support

**Decision**: Defer Windows support (focus on Unix/Linux/macOS)

**Rationale**:
- Terminal handling is significantly different on Windows
- `/dev/tty` doesn't exist, would need `CONIN$`
- Raw mode implementation differs
- MVP focuses on most common development environments

## UI/UX Decisions

### Color Usage

**Decision**: Minimal color usage - only green for NOLINT comments

**Rationale**:
- Clear visual distinction without overwhelming
- Works on most terminals
- Accessible and not distracting

### Clear Screen Behavior

**Decision**: Clear screen between warnings using ANSI codes

**Rationale**:
- Clean presentation
- Focus on one warning at a time
- Reduces cognitive load

## Lessons Learned

1. **Terminal I/O is Complex**: What seems simple (reading a single key) involves multiple system-level considerations

2. **File Descriptors Matter**: Setting raw mode on the wrong file descriptor silently fails

3. **Buffer Management**: Multiple levels of buffering (FILE*, terminal) can interfere with each other

4. **RAII is Essential**: Terminal state must be restored reliably - RAII prevents terminal corruption

5. **MVP First**: Starting with basic inline comments allowed us to get core functionality working before adding complexity

6. **Test with Real Data**: Using actual clang-tidy output immediately revealed issues with parsing and file access

## Future Considerations

- **Persistent State**: Save progress for large codebases
- **Undo Functionality**: Track changes for rollback
- **Smarter Placement**: Use AST information for better NOLINTBEGIN/END placement
- **Integration**: Direct clang-tidy execution for convenience
- **Configuration**: User preferences for default styles, colors, etc.