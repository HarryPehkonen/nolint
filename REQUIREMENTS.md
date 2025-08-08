# NOLINT - Requirements Document

## Overview
`nolint` is a command-line tool that automatically applies `// NOLINT` comments to C++ source code based on clang-tidy warnings. It provides an interactive interface for reviewing and selectively suppressing linting warnings.

**  TARGET REQUIREMENTS**: All requirements below define the target functionality for the rewritten system. The implementation will feature comprehensive interactive capabilities, robust error handling, and extensive test coverage.

## Functional Requirements

### Core Features

#### 1. Warning Processing
- Read clang-tidy output from:
  - Input file (`--input <file>`)
  - Standard input (pipe mode)
- Parse clang-tidy warning format to extract:
  - File path
  - Line number
  - Column number
  - Warning type/category (e.g., `readability-magic-numbers`)
  - Warning message
  - Function size from note lines (e.g., "note: 44 lines including whitespace")

#### 2. Interactive Review
- Display each warning with context:
  - Show lines before the warning location (default: 5, configurable with `-B`)
  - Show lines after the warning location (default: 5, configurable with `-A`)
  - Highlight the specific line with the warning
  - **Critical**: For NOLINT_BLOCK mode, show split display with actual function boundaries and gap indicator
  - **Visual Preview**: Show exactly where suppression comments will be placed using `+` prefix
- Present user options:
  - **↑/↓** - Cycle through different NOLINT formats (auto-saved with live preview)  
  - **←/→** - Navigate between warnings (bidirectional with decision memory)
  - **x** - Save all changes and exit with summary
  - **q** - Quit without saving (with y/n confirmation)
  - **/** - Search/filter warnings by type or content
  - **t** - Show warning type statistics with interactive navigation
  - **Key Feature**: All style changes auto-saved immediately when navigating
  - **Critical Feature**: Navigation remembers and preserves all previous decisions
  - **Real-time Status**: Displays suppression count and filter status continuously

#### 3. Suppression Comment Types
Support multiple NOLINT formats (cycled with arrow keys):
1. **NONE** - No suppression (allows "undo" via style selection)
2. `// NOLINT(warning-name)` - Suppress specific warning on the line  
3. `// NOLINTNEXTLINE(warning-name)` - Suppress warning on the next line
4. `// NOLINTBEGIN(warning-name)` and `// NOLINTEND(warning-name)` - Block suppression

**Critical Design Decisions**:
- **Conditional NOLINT_BLOCK**: Only show for warnings with `function_lines` metadata (e.g., readability-function-size)
- **Smart Placement**: NOLINTBEGIN placed right before function signature, not before access specifiers
- **Indentation Matching**: Extract indentation from target line, not warning line
- **Style Cycling Order**: NONE → NOLINT_SPECIFIC → NOLINTNEXTLINE → NOLINT_BLOCK (if applicable) → NONE

For NOLINTBEGIN/END placement:
- **Function-level warnings**: Use reported line count and smart function boundary detection
- **Placement Logic**: NOLINTBEGIN before function signature, NOLINTEND after function closing brace
- **Display Enhancement**: Show split view with actual code lines and gap separator (`=== N more lines ==`)

#### 4. **Statistics and Analysis ('t' key)**

**  FULLY IMPLEMENTED**: Comprehensive warning type analysis with interactive navigation and filtering integration.

**Core Statistics Features**:
- **Warning Type Summary**: Display all warning types with total counts, addressed counts, and visited counts
- **Progress Tracking**: Shows percentage of each warning type that has been addressed with NOLINT suppressions
- **Visit Tracking**: Tracks which warnings have been displayed to the user during the session
- **Color-Coded Display**: Visual indicators for suppression status:
  - **Green**: 100% of warnings addressed (fully suppressed)
  - **Yellow**: Partially addressed (some warnings suppressed)
  - **Red**: Not addressed (no suppressions applied)

**Interactive Navigation**:
- **↑/↓ arrows**: Navigate between warning types in statistics view
- **Enter**: Filter main view to show only the selected warning type
- **Escape**: Return to main warning review interface
- **Real-time updates**: Statistics recalculated as user makes suppression decisions

**Statistics Display Format**:
```
=== Warning Type Summary ===
Total: 47 warnings | Addressed: 12 (26%) | Visited: 23

┌─────────────────────────────────────┬─────────┬─────────────┬─────────┐
│ Warning Type                        │  Total  │  Addressed  │ Visited │
├─────────────────────────────────────┼─────────┼─────────────┼─────────┤
│ >> readability-function-size        │     8   │    3 (38%)  │    5    │
│    readability-magic-numbers        │    15   │   10 (67%)  │   12    │
│    modernize-use-auto               │    24   │    0 (0%)   │    6    │
└─────────────────────────────────────┴─────────┴─────────────┴─────────┘
```

**Integration with Main Workflow**:
- Statistics accessible from any warning via 't' key
- Filter integration allows immediate focus on specific warning types
- Progress persists across navigation and filter operations
- Statistics update in real-time as suppressions are applied

#### 5. **Search and Filtering ('/' key)**

**  FULLY IMPLEMENTED**: Powerful multi-term search functionality with cross-field matching and robust bounds checking.

**Search Features**:
- **Multi-term AND logic**: Space-separated terms must all match (e.g., "magic 42" finds warnings containing both "magic" AND "42")
- **Cross-field searching**: Searches across file path, warning type, message, and line numbers
- **Case-insensitive matching**: Automatically converts search terms to lowercase
- **Real-time filter status**: Shows "X/Y warnings (filtered: 'term')" in status line
- **Live character echoing**: Search input fully visible even in raw terminal mode

**Search Workflow**:
1. Press '/' key from any warning to enter search mode
2. Type search terms (space-separated for AND logic)
3. Press Enter to apply filter
4. Navigate filtered results with ←/→ arrows
5. Enter empty search to clear filter and show all warnings

**Bounds Checking Integration**:
- **Safe navigation**: Prevents crashes when navigating filtered results
- **Automatic adjustment**: Current index adjusted when filter changes result set
- **Graceful fallbacks**: Shows all warnings if filter produces no matches
- **Clear feedback**: "No warnings match filter" message with automatic reversion

**Filter Status Display**:
```
Showing 5/47 warnings (filtered: 'magic numbers') | Use / to search
```

**Advanced Search Examples**:
- `readability` - Shows all readability warnings
- `magic 42` - Shows warnings containing both "magic" AND "42"
- `src/parser.cpp` - Shows warnings only from specific file
- `line 150` - Shows warnings near line 150

#### 6. **Decision Tracking and Modification**  

**  FULLY IMPLEMENTED**: Users can navigate bidirectionally and modify any previous decision with full choice memory.

**Architecture Implementation**:
- **  Deferred Modification**: Changes tracked separately in `warning_decisions_` map
- **  Decision History**: All decisions maintained with ability to modify via navigation
- **  Preview Mode**: Real-time preview shows exact code with green NOLINT highlighting
- **  Conflict Resolution**: Robust handling of multiple warnings per line
- **  Batch Application**: All decisions applied atomically on save

**User Experience Implementation**:
- **  Navigate and modify**: Arrow navigation preserves and allows changing any decision  
- **  Immediate preview**: Real-time display of code transformations with NOLINT comments
- **  Save summary**: Shows count of suppressions applied on exit
- **  Undo capability**: Setting style to NONE removes any suppression
- **  Status indicators**: Suppression counter and filter status always visible

**Technical Requirements**:
```cpp
// Each file line tracks original content + pending modifications
struct LineWithDecisions {
    std::string original_text;
    std::map<int, PendingModification> warning_decisions;
    auto get_preview_text() const -> std::string;
};

// Global decision tracking for navigation
class DecisionTracker {
    void record_decision(int warning_id, NolintStyle style);
    void change_decision(int warning_id, NolintStyle new_style);
    auto get_file_preview() -> std::vector<std::string>;
    auto apply_all_decisions() -> std::vector<std::string>;
};
```

#### 5. File Modification Strategy
- Load entire file into memory (`std::vector<std::string>`) on first warning
- **Track decisions separately** from file content (metadata approach)
- **Preserve original file** unchanged until final commit
- Track line number calculations dynamically for preview
- Write files only when:
  - User selects `X` (exit and save) - applies ALL pending decisions
  - User selects `S` (save current file) - applies decisions for that file
- Support backup files before modification (optional)

### Command-Line Interface

#### Basic Usage
```bash
# From file (interactive mode)
nolint --input warnings.txt

# Pipe mode (interactive with /dev/tty)
clang-tidy src/*.cpp | nolint

# Non-interactive with default style
nolint --style nextline --non-interactive < warnings.txt

# Dry run to preview changes
nolint --input warnings.txt --dry-run
```

#### Options
- `-i, --input <file>` - Read warnings from file (default: stdin)
- `-s, --style <style>` - Default suppression style: specific|nextline|block (default: specific)
- `-n, --non-interactive` - Apply default style to all warnings without prompting
- `--dry-run` - Show what would be changed without modifying files
- `--force` - Allow file modifications when input is piped (unsafe)
- `-h, --help` - Show help message

**Note**: Interactive filtering and statistics features are available during interactive mode using '/' and 't' keys respectively.

## Non-Functional Requirements

### Performance
- Efficient handling of large files (keep in memory only while processing)
- Process thousands of warnings without significant slowdown
- Lazy file loading (only load when needed)
- Batch write operations for efficiency

### Usability
- Clear, colorized output for better readability
- Responsive arrow key navigation without delay
- Progress indicators: `[3/47] Processing readability-magic-numbers in foo.cpp:42`
- Visual indication of selected NOLINT format
- Show pending changes count: `[2 files modified, 5 warnings suppressed]`
- Confirmation prompt before exit if unsaved changes exist

### Reliability
- Graceful handling of:
  - Malformed clang-tidy output (skip and continue)
  - Missing or inaccessible files (report and skip)
  - Write-protected files (report error, keep in memory)
  - Interrupted operations (offer to save on SIGINT)
- Never partially modify files (atomic writes)
- Maintain consistent state across multiple warnings per file

### Compatibility
- Support major platforms: Linux, macOS, Windows
- Parse clang-tidy output from versions 10+
- Handle various text encodings (UTF-8, ASCII)
- Support different line endings (LF, CRLF)
- Work with different C++ standards and styles

## Example Workflow

```
$ clang-tidy src/*.cpp | nolint

=== Interactive NOLINT Tool ===
Suppressions: 0 | Use ←→ to navigate, ↑↓ to change style
Showing 15/15 warnings | Use / to search

┌─ Warning 1/15 ─
│ File: src/main.cpp
│ Line: 42:15
│ Type: readability-magic-numbers
│ Message: 75 is a magic number, consider using a named constant instead
│
│     39 | void process_data() {
│     40 |     const int iterations = 100;
│     41 |     for (int i = 0; i < iterations; ++i) {
│ >>  42 |         if (data[i] > 75) {  // NOLINT(readability-magic-numbers)
│     43 |             handle_threshold(data[i]);
│     44 |         }
│     45 |     }
│
│ Apply NOLINT? Format: // NOLINT(readability-magic-numbers)
└─
Navigate [←→] Style [↑↓] Save & Exit [x] Quit [q] Search [/] Stats [t]: 

> →  (Navigate to next warning)

┌─ Warning 2/15 ─
│ File: src/parser.cpp  
│ Line: 78:10
│ Type: readability-function-size
│ Message: function exceeds recommended size/complexity thresholds
│
│     75 | // Complex parsing function
│     76 | // TODO: Consider refactoring
│     77 |
│ >>  78 | void Parser::parse_expression(const Token* tokens, size_t count) {
│    ... | (42 lines skipped)
│    122 | }
│
│ Apply NOLINT? Format: // NOLINTBEGIN(readability-function-size) ... // NOLINTEND(readability-function-size)
└─
Navigate [←→] Style [↑↓] Save & Exit [x] Quit [q] Search [/] Stats [t]: 

> t  (Show statistics)

=== Warning Type Summary ===
Total: 15 warnings | Addressed: 2 (13%) | Visited: 2

┌─────────────────────────────────────┬─────────┬─────────────┬─────────┐
│ Warning Type                        │  Total  │  Addressed  │ Visited │
├─────────────────────────────────────┼─────────┼─────────────┼─────────┤
│ >> readability-function-size        │     3   │    1 (33%)  │    1    │
│    readability-magic-numbers        │     8   │    1 (13%)  │    1    │
│    modernize-use-auto               │     4   │    0 (0%)   │    0    │
└─────────────────────────────────────┴─────────┴─────────────┴─────────┘

Navigate [↑↓] Filter [Enter] Back [Escape]: 

> Escape  (Return to warnings)

Navigate [←→] Style [↑↓] Save & Exit [x] Quit [q] Search [/] Stats [t]: 

> x  (Save and exit)

Saving changes and exiting...
Successfully applied 2 suppressions.
```

## Architecture Philosophy

### **Functional Reactive Architecture for Interactive UI**

The interactive UI should implement a **Model-View-Update pattern** (functional reactive architecture) with:

#### **1. Immutable State Model**
All UI state must live in a single immutable struct:
```cpp
struct UIModel {
    std::vector<Warning> warnings;
    int current_index = 0;
    NolintStyle current_style = NolintStyle::NONE;
    std::unordered_map<std::string, NolintStyle> warning_decisions;
    std::string current_filter;
    bool in_statistics_mode = false;
    bool should_save_and_exit = false;
    // ... all other UI state
};
```

#### **2. Pure State Transitions**  
Update functions must be pure - same input always produces same output:
```cpp
auto update(const UIModel& current, const InputEvent& event) -> UIModel {
    UIModel next = current;  // Copy current state (immutable)
    
    switch (event) {
        case InputEvent::ARROW_RIGHT:
            if (current.current_index < warnings.size() - 1) {
                next.current_index++;
            }
            break;
        // ... other pure state transitions
    }
    
    return next;  // Return new immutable state
}
```

#### **3. Explicit Rendering**
Render function must display current state without modifying it:
```cpp
auto render(const UIModel& model) -> void {
    // Pure rendering - no state changes!
    display_warning_context(model);
    display_status_line(model);
    display_controls(model);
}
```

#### **4. Unidirectional Data Flow**
Main loop must follow strict Input → Update → Render cycle:
```cpp
auto run_interactive() -> void {
    UIModel model = initialize_model();
    
    while (!should_exit(model)) {
        render(model);                              // 1. Explicit render
        auto input_event = terminal_->get_input();  // 2. Get input event  
        model = update(model, input_event);         // 3. Pure state update
        // Repeat cycle - no nested loops!
    }
}
```

### **Benefits of This Architecture:**
- **No hidden state mutations** scattered across functions
- **No complex nested loops** with unclear refresh timing  
- **No state synchronization bugs** between display and model
- **Predictable behavior** - same input always produces same state transition
- **Easy testing** - pure update functions can be tested in isolation
- **Clear separation** - rendering completely separate from state management

## Critical Edge Case Requirements

### **Multiple Suppressions on Same Line**

**REQUIREMENT**: When both NOLINT_BLOCK and NOLINTNEXTLINE target the same line, the system MUST maintain correct ordering to prevent suppression failures.

**Correct Order** (MANDATORY):
1. `NOLINTBEGIN` comment (starts block)
2. `NOLINTNEXTLINE` comment (suppresses next line)  
3. Target code line (both suppressions active)
4. Subsequent lines (only block suppression active)
5. `NOLINTEND` comment (ends block)

**Example Requirement**:
```cpp
// Given these user decisions:
// Warning A: readability-function-size (function lines 10-20) → NOLINT_BLOCK
// Warning B: readability-magic-numbers (line 10) → NOLINTNEXTLINE

// System MUST generate:
// NOLINTBEGIN(readability-function-size)    ← FIRST
// NOLINTNEXTLINE(readability-magic-numbers) ← SECOND  
void big_function() {  // Line 10 - both suppressions work
    int x = 42;        // Line 11 - only block suppression active
    // ...
}
// NOLINTEND(readability-function-size)

// System MUST NOT generate:
// NOLINTNEXTLINE(readability-magic-numbers) ← WRONG ORDER
// NOLINTBEGIN(readability-function-size)    ← Would break NOLINTNEXTLINE
```

### **AnnotatedFile Architecture Requirement**

**REQUIREMENT**: Use AnnotatedFile model to prevent line number drift and handle edge cases.

```cpp
// REQUIRED data structures:
struct AnnotatedLine {
    std::string text;                            // Original line content
    std::vector<std::string> before_comments;    // ORDERED: NOLINTBEGIN first, NOLINTNEXTLINE second
    std::optional<std::string> inline_comment;   // Inline NOLINT
};

struct BlockSuppression {
    size_t start_line;      // Original line number (immutable)
    size_t end_line;        // Original line number (immutable) 
    std::string warning_type;
};

struct AnnotatedFile {
    std::vector<AnnotatedLine> lines;           // Original structure preserved
    std::vector<BlockSuppression> blocks;       // NOLINTBEGIN/END pairs
};
```

**REQUIREMENT**: Pure render function with mandatory ordering:
```cpp
auto render_annotated_file(const AnnotatedFile& file) -> std::vector<std::string> {
    // MUST follow this exact order for each line:
    // 1. NOLINTBEGIN blocks (if any start at this line)
    // 2. NOLINTNEXTLINE comments (if any target this line)
    // 3. Original code line (with optional inline NOLINT)
    // 4. NOLINTEND blocks (if any end at this line)
}
```

### **Test Requirements for Edge Cases**

**MANDATORY TEST**: Multiple suppressions same line
```cpp
TEST(EdgeCases, NolintBlockPlusNolintnextlineSameLine) {
    AnnotatedFile file = create_test_file();
    
    // Apply NOLINT_BLOCK for function
    apply_decision(file, warning_function_size, NolintStyle::NOLINT_BLOCK);
    
    // Apply NOLINTNEXTLINE for same line
    apply_decision(file, warning_magic_numbers, NolintStyle::NOLINTNEXTLINE);
    
    auto rendered = render_annotated_file(file);
    
    // MUST have correct order
    EXPECT_THAT(rendered, Contains("// NOLINTBEGIN(readability-function-size)"));
    EXPECT_THAT(rendered, Contains("// NOLINTNEXTLINE(readability-magic-numbers)"));
    
    // NOLINTBEGIN must come before NOLINTNEXTLINE
    auto begin_pos = find_line_containing(rendered, "NOLINTBEGIN");
    auto next_pos = find_line_containing(rendered, "NOLINTNEXTLINE");
    EXPECT_LT(begin_pos, next_pos);
}
```

**MANDATORY TEST**: Line number drift prevention
```cpp
TEST(EdgeCases, NoLineNumberDrift) {
    AnnotatedFile file = create_large_test_file(100);  // 100 lines
    
    // Apply many suppressions
    for (int i = 0; i < 50; ++i) {
        apply_decision(file, create_warning(i * 2), random_style());
    }
    
    auto rendered = render_annotated_file(file);
    auto original_lines = extract_non_comment_lines(rendered);
    
    // Original line count must be preserved
    EXPECT_EQ(original_lines.size(), 100);
    
    // Original content must be unchanged  
    EXPECT_EQ(original_lines[0], file.lines[0].text);
    EXPECT_EQ(original_lines[99], file.lines[99].text);
}
```

### **Functional Programming for Core Logic**  
The text processing core should remain purely functional:

1. **Pure Functions for Business Logic**:
   - Text transformations: `apply_modification_to_lines(lines, modification) -> transformed_lines`
   - Warning analysis: `find_function_boundaries(file_lines, warning) -> (start, end)`  
   - Context building: `build_context(warning, file_lines, style) -> context`
   - Indentation extraction: `extract_indentation(line) -> indent_string`

2. **Separation of Pure Core from I/O Shell**:
   ```
   ┌─────────────────────────────────────────┐
   │           I/O Shell                     │  <- Side effects, minimal testing
   │  ┌─────────────────────────────────────┐ │  <- Functional Reactive UI  
   │  │     **Functional Core**             │ │  <- Pure functions, extensive testing
   │  │  • Text transformations             │ │  
   │  │  • Warning processing               │ │  
   │  │  • Context building                 │ │  
   │  └─────────────────────────────────────┘ │
   └─────────────────────────────────────────┘
   ```

3. **Testing Benefits**:
   - **No mocking needed** for core business logic or UI state transitions
   - **Fast tests** (no I/O operations)  
   - **Deterministic behavior** (no hidden state)
   - **Easy edge case testing** (pure input → output)

### Terminal I/O Complexity Lessons
- **Raw mode terminal handling is non-trivial**: Requires careful resource management
- **Platform differences**: `/dev/tty` on Unix, different approach needed on Windows  
- **Buffer management**: Multiple layers of buffering can interfere
- **RAII essential**: Terminal state must be restored reliably
- **Single-key input**: Use `tcsetattr()` with `VMIN=1, VTIME=0` for immediate response

##   Implementation Status: COMPLETE WITH ADVANCED FEATURES

**All core requirements plus advanced analysis features have been fully implemented:**

### **Core Interactive Features**
-   **Interactive review** with real-time preview and green NOLINT highlighting
-   **Multiple suppression styles** with intelligent conditional availability
-   **Bidirectional navigation** (←/→) with comprehensive choice memory
-   **Style cycling** (↑/↓) with auto-save and live preview updates
-   **Save and exit** ('x') with atomic modifications and summary
-   **Quit with confirmation** ('q') preventing accidental data loss

### **Advanced Analysis and Filtering**
-   **Warning Type Statistics** ('t' key):
  - Real-time progress tracking (total, addressed, visited)
  - Color-coded display (green/yellow/red for completion status)
  - Interactive navigation with filtering integration
  - Seamless mode switching with choice preservation

-   **Multi-term Search and Filtering** ('/' key):
  - Cross-field search (file path, warning type, message, line numbers)
  - Multi-term AND logic with space-separated terms
  - Live character echoing in raw terminal mode
  - Robust bounds checking prevents navigation crashes
  - Filter status display with real-time counts

### **User Experience and Feedback Requirements**
-   **Boundary Navigation Feedback**:
  - Clear messages when at navigation limits: "Already at first warning" / "Already at last warning"
  - Prevents user confusion and provides immediate feedback
  - Consistent messaging across all navigation contexts

-   **Filter Application Feedback and Graceful Failure**:
  - Specific feedback messages for filter operations:
    - Success: "Applied filter: 'term' - showing X/Y warnings"
    - No matches: "No warnings match filter 'term' - showing all X warnings" (auto-revert)
    - Clear: "Filter cleared - showing all X warnings"
  - Graceful handling of empty filter results with automatic fallback
  - Real-time count updates in all feedback messages

-   **Data Safety and Exit Confirmation**:
  - **Quit confirmation** ('q' key): Requires explicit y/n confirmation to prevent accidental data loss
  - **Save protection**: Changes only applied on explicit save ('x' key) 
  - **Clear warnings**: "Are you sure you want to quit without saving? [y/n]"
  - **Graceful cancellation**: User can cancel quit operation and continue working

### **Technical Excellence**
-   **Functional core architecture** with pure transformation functions
-   **Comprehensive session state management** with complete persistence
-   **Terminal handling** with `/dev/tty` support and proper state restoration
-   **Memory safety** with extensive bounds checking and crash prevention
-   **Extensive test coverage** with regression testing for critical bugs
-   **Modern C++20 implementation** with ranges, concepts, and RAII patterns

### **User Experience Excellence**
-   **Single-key controls** with immediate response (no Enter required)
-   **Auto-save functionality** preserves all decisions when navigating
-   **Real-time status indicators** (suppression count, filter status, progress)
-   **Seamless integration** between warnings, statistics, and search modes
-   **Comprehensive help** and clear feedback messages
-   **Choice memory preservation** across all operations and mode switches

**Production Ready**: The tool is fully functional with advanced analysis capabilities and is actively used for comprehensive clang-tidy suppression management with filtering and progress tracking.

## Future Enhancements (Optional)
- Configuration file support (`.nolintrc`)
- Smart function detection using clang AST
- Integration with version control (show diff before saving)
- Batch mode for CI/CD pipelines  
- Statistics export (CSV/JSON format)
