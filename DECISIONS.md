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

### 11. NOLINT_BLOCK Conditional Availability

**Problem**: NOLINTBEGIN/END blocks should only be offered for warnings that span multiple lines

**Decision**: Only show NOLINT_BLOCK style for warnings with `function_lines` metadata
- Function-size warnings include line count from clang-tidy: "Note: 70 lines including whitespace"
- Single-line warnings (magic numbers, naming, etc.) cycle between NOLINT and NOLINTNEXTLINE only

**Implementation**: Check `warning.function_lines.has_value()` in style cycling logic

### 12. Previous Navigation (P Key)

**Problem**: Users navigate through warnings quickly and might miss ones they want to review

**Decision**: Add P)revious key for backward navigation
- Allows going back to previous warnings
- Preserves any modifications already made
- Enables changing decisions on warnings already processed

**Implementation**: Modified main processing loop to handle bidirectional navigation

### 13. "No Suppression" as Style Option

**Problem**: Users might want to undo NOLINT decisions when reviewing with P)revious

**Decision**: Add `NolintStyle::NONE` as a cycle option
- Appears as "(no suppression)" in format display
- Accessible via arrow keys like other styles
- Effectively skips applying any NOLINT when selected
- Provides way to "undo" without separate undo command

**Rationale**: Simpler UX than dedicated undo key - everything is style selection

### 14. ✅ **RESOLVED**: Previous Navigation with Decision Memory

**Problem**: Users needed to navigate backwards and modify previous decisions

**Implementation**: **Decision Memory with Deferred Application**
- Added `warning_decisions_` map to track choices by warning key
- Decisions are remembered when navigating with 'p' key
- All modifications deferred until save ('x') or automatic apply at end
- Users can change any previous decision by navigating back

**Code Solution**:
```cpp
// Remember choices by warning location
std::unordered_map<std::string, NolintStyle> warning_decisions_;

// Generate unique key for each warning
auto get_warning_key(const Warning& warning) -> std::string {
    return warning.file_path + ":" + std::to_string(warning.line_number) + 
           ":" + std::to_string(warning.column_number);
}

// Restore previous choice when navigating back
auto prev_decision = warning_decisions_.find(warning_key);
if (prev_decision != warning_decisions_.end()) {
    current_style = prev_decision->second;
}
```

**User Experience Flow**:
1. Navigate through warnings making decisions
2. Use 'p' to go back - previous choice is restored
3. Change decision with arrow keys if desired
4. Use 'x' to save all changes atomically

**Result**: Users can now freely navigate backwards and forwards, with all choices preserved

### 15. ✅ **IMPLEMENTED**: Integrated Preview System

**Problem**: Users needed to see what the code would look like with NOLINT applied

**Solution**: **Real-time Integrated Preview**
- `build_display_context()` applies transformations and shows actual result
- Green highlighting for NOLINT comments using ANSI escape codes
- Line markers show added content (`+` prefix for NOLINTNEXTLINE)
- Preview updates immediately when cycling through styles

**Implementation**:
```cpp
// Apply transformation and show result
auto mod = create_modification(warning, current_style, file_lines);
auto transformation = apply_modification_to_lines(file_lines, mod);
display_lines = std::move(transformation.lines);

// Highlight NOLINT comments in green
auto highlight_nolint_comments(const std::string& line) -> std::string {
    const std::string GREEN = "\033[32m";
    const std::string RESET = "\033[0m";
    // Find and highlight NOLINT patterns...
}
```

**Result**: Users see exactly what their code will look like before making the decision

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

## Critical Implementation Lessons

### Terminal I/O Complexity

1. **`/dev/tty` vs `stdin` Separation**: 
   - **Problem**: Piped input blocks keyboard interaction
   - **Solution**: Open `/dev/tty` for user input, keep `stdin` for data
   - **Key**: `fopen("/dev/tty", "r+")` for read/write mode

2. **Single-Key Input Challenges**:
   - **Failed**: Line-buffered input with `fgets()`
   - **Failed**: Raw mode on wrong file descriptor  
   - **Success**: Raw mode on `/dev/tty` with `tcsetattr()`
   - **Critical**: Disable buffering with `setbuf(tty, nullptr)`

3. **Terminal State Restoration**:
   - **Problem**: Terminal corruption requiring `reset` command
   - **Solution**: Multiple cleanup mechanisms:
     ```cpp
     std::atexit(restore_terminal_on_exit);           // Normal exit
     std::signal(SIGINT, restore_terminal_on_signal); // Ctrl+C
     ~Terminal() { /* restore in destructor */ }      // RAII
     ```

4. **Arrow Key Detection**:
   - **Challenge**: Arrow keys are 3-character escape sequences (`\x1b[A`)
   - **Solution**: Sequential character reading in raw mode
   - **Implementation**: Read ESC, then `[`, then direction character

### User Experience Insights

5. **Real-time Preview is Essential**:
   - Users need to see actual code transformation, not format strings
   - Live preview updates as user cycles through styles
   - Green highlighting makes NOLINT comments immediately visible

6. **Navigation Memory is Critical**:
   - Users frequently go back to change previous decisions
   - Choice memory prevents frustration and re-work
   - Deferred application allows safe navigation

7. **Single-key Commands Dramatically Improve UX**:
   - Removing Enter requirement speeds up interaction by 10x
   - Users can quickly cycle through many warnings
   - Immediate feedback creates fluid experience

### Development Process Lessons

8. **Test with Real Data Early**: 
   - Synthetic test data missed edge cases in actual clang-tidy output
   - Real file paths exposed path handling bugs
   - Large warning sets revealed performance issues

9. **RAII Prevents Subtle Bugs**:
   - Terminal state corruption is hard to debug
   - Resource cleanup must be automatic and reliable
   - Multiple cleanup paths (destructor, signal handlers, atexit) ensure robustness

10. **Functional Core Architecture Pays Off**:
    - Pure text transformation functions are easy to test
    - No mocking needed for core business logic
    - Complex UI interactions separated from text processing
    - 60 tests with high confidence in correctness

## Navigation Flow Evolution

**Initial Design**: Linear forward-only navigation (Y/N/Q/X/S)
**Final Implementation**: Bidirectional navigation with style flexibility and single-key controls

**✅ Implemented Controls**:
1. **s** - Accept current style and advance (single key, no Enter)
2. **k** - Skip warning and advance (single key, no Enter)
3. **p** - Go back to previous warning (remembers previous choice)
4. **↑/↓** - Cycle through styles with live preview
5. **x** - Save all changes and exit
6. **q** - Quit without saving changes

**Style Cycling Order**:
- NOLINT_SPECIFIC → NOLINTNEXTLINE → NOLINT_BLOCK (function warnings only) → NONE → (repeat)

**Key Breakthrough**: Single-key commands with `/dev/tty` and raw terminal mode for immediate response

## Architecture and Testing Philosophy

### Current Architecture Assessment

**What Works Well:**
- Dependency injection with interfaces (IFileSystem, IUserInterface)
- Separation of parsing, file management, and UI concerns
- RAII for resource management (terminal state, file transactions)

**Testing Pain Points Identified:**
1. **Stateful Components**: FileManager holds mutable state that accumulates across operations
2. **Mixed Concerns**: Business logic (text transformation) mixed with I/O operations
3. **Side Effects**: Functions modify objects instead of returning new values
4. **Testing Complexity**: Need mocks for simple text transformations
5. **Temporal Coupling**: Order of operations matters due to internal state

### Proposed Functional Architecture

**Core Principle**: Separate pure functions (business logic) from I/O shell (side effects)

```
┌─────────────────────────────────────────┐
│           I/O Shell                     │  <- Handles all side effects
│  ┌─────────────────────────────────────┐ │  <- Mocking needed here only
│  │        Functional Core              │ │  <- Pure functions
│  │  • Text transformations             │ │  <- No mocking needed
│  │  • Warning processing               │ │  <- Trivial to test  
│  │  • Context building                 │ │  <- Fast tests
│  │  • Modification creation            │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

**Benefits of Functional Approach:**

1. **Testability**: 
   - Pure functions need no mocks - just input → output verification
   - Tests run in milliseconds (no I/O)
   - Easy to test edge cases and error conditions
   - Deterministic behavior - no flaky tests

2. **Maintainability**:
   - Business logic isolated from I/O concerns  
   - Easy to reason about - no hidden state
   - Composable functions - easy to extend
   - Immutable data structures prevent bugs

3. **Performance**:
   - Tests can run in parallel (no shared state)
   - Easy to optimize pure functions
   - Batch I/O operations for efficiency

**Example Test Comparison:**

```cpp
// Current Architecture (Complex)
TEST_F(FileManagerTest, ApplyNolintBlock) {
    // Set up mock file system
    auto mock_fs = std::make_unique<MockFileSystem>();
    EXPECT_CALL(*mock_fs, read_file("/test.cpp"))
        .WillOnce(Return(file_contents));
    EXPECT_CALL(*mock_fs, write_file("/test.cpp", _))
        .WillOnce([&result](const std::string&, const auto& lines) {
            result = lines; // Capture side effect
        });
    
    auto file_manager = std::make_unique<FileManager>(std::move(mock_fs));
    // ... 15 more lines of setup ...
}

// Functional Architecture (Simple)  
TEST_F(FunctionalTest, ApplyNolintBlock) {
    auto result = apply_modification_to_lines(original_lines, modification);
    
    EXPECT_EQ(result.lines[5], "    // NOLINTBEGIN(warning-type)");
    EXPECT_EQ(result.lines[10], "    // NOLINTEND(warning-type)");
}
```

### Implementation Strategy

**Phase 1: Extract Pure Functions**
- Move text transformation logic to `functional_core` namespace
- Keep existing architecture for I/O and UI
- Add functional tests alongside existing tests

**Phase 2: Refactor I/O Shell**
- Create stateless I/O coordinators
- Batch file operations for efficiency
- Use functional core for all business logic

**Phase 3: Simplify Testing**
- Replace complex integration tests with simple unit tests
- Keep minimal integration tests for I/O edge cases
- Focus test coverage on the functional core

### Functional Programming Benefits for This Domain

**Text Processing is Naturally Functional:**
- Input: Lines of text + Modification rules
- Output: Transformed lines of text
- No need for mutable state or side effects

**Error Handling Becomes Explicit:**
```cpp
// Instead of exceptions and state corruption
auto result = apply_modification_to_lines(lines, mod);
if (result.lines.empty()) {
    // Handle error case cleanly
}
```

**Composition and Reusability:**
```cpp
// Pure functions compose naturally
auto context = build_context(warning, file_lines, style);
auto transformation = apply_modification_to_lines(file_lines, mod);
auto formatted = apply_line_endings(transformation.lines, ending);
```

### Testing Strategy Recommendations

1. **80/20 Rule**: 80% of tests should be pure function tests (fast, no mocks)
2. **I/O Boundary Tests**: Test file I/O separately with minimal scenarios
3. **Property-Based Testing**: Use randomized inputs to test text transformations
4. **Snapshot Testing**: Capture expected outputs for complex scenarios
5. **Performance Testing**: Pure functions are easy to benchmark

## Future Considerations

- **Persistent State**: Save progress for large codebases
- **True Undo**: Track and remove previously applied NOLINT comments when changing from something to NONE
- **Smarter Placement**: Use AST information for better NOLINTBEGIN/END placement
- **Integration**: Direct clang-tidy execution for convenience  
- **Configuration**: User preferences for default styles, colors, etc.
- **Session Resume**: Continue where left off in large projects
- **Functional Refactoring**: Migrate to functional architecture for improved testability