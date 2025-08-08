# NOLINT - Architecture Documentation

## Overview

`nolint` implements a **Functional Core / I/O Shell** architecture that separates pure business logic from side effects, resulting in highly testable code with excellent maintainability.

## Architecture Pattern

```
┌─────────────────────────────────────────┐
│           I/O Shell                     │  ← Side effects (File I/O, Terminal, UI)
│  ┌─────────────────────────────────────┐ │  ← Minimal testing needed
│  │        Functional Core              │ │  ← Pure functions  
│  │  • Text transformations             │ │  ← Extensively tested
│  │  • Warning processing               │ │  ← No mocking required
│  │  • Context building                 │ │  ← Fast, deterministic tests
│  │  • Modification creation            │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

## Functional Core (`functional_core.hpp`)

The functional core contains all business logic as **pure functions** - functions that:
- Take inputs and return outputs with no side effects
- Are deterministic and easy to test
- Can be composed and reused

### Key Pure Functions

```cpp
namespace functional_core {
    // AnnotatedFile creation and manipulation - prevents line number drift
    auto create_annotated_file(const std::vector<std::string>& lines) -> AnnotatedFile;
    
    auto apply_decision(AnnotatedFile& file, const Warning& warning, NolintStyle style) -> void;
    
    auto render_annotated_file(const AnnotatedFile& file) -> std::vector<std::string>;
    
    // Context building for UI display
    auto build_display_context(
        const Warning& warning,
        const AnnotatedFile& file,
        NolintStyle current_style
    ) -> DisplayContext;
    
    // Warning filtering and search
    auto filter_warnings(
        const std::vector<Warning>& warnings,
        const std::string& filter
    ) -> std::vector<size_t>;  // Return indices, not copies
    
    // Text utilities
    auto extract_indentation(const std::string& line) -> std::string;
}

// Core data structures that prevent line number drift
struct AnnotatedLine {
    std::string text;                            // Original line content
    std::vector<std::string> before_comments;    // ORDERED: NOLINTBEGIN first, then NOLINTNEXTLINE
    std::optional<std::string> inline_comment;   // Inline NOLINT
};

struct BlockSuppression {
    size_t start_line;      // Original line number (never changes!)
    size_t end_line;        // Original line number (never changes!)
    std::string warning_type;
};

struct AnnotatedFile {
    std::vector<AnnotatedLine> lines;           // Original structure preserved
    std::vector<BlockSuppression> blocks;       // NOLINTBEGIN/END pairs
};
```

### Benefits of Pure Functions

1. **Easy Testing**: No mocking required - just input → output verification
2. **Fast Tests**: No I/O operations, tests run in milliseconds
3. **Deterministic**: Same input always produces same output
4. **Composable**: Functions can be easily combined and reused
5. **Debuggable**: No hidden state or side effects to track

### AnnotatedFile Approach Benefits

1. **Prevents Line Number Drift**: Original line numbers never change, eliminating bugs from line insertion
2. **Handles Edge Cases**: NOLINTBEGIN + NOLINTNEXTLINE on same line handled correctly with proper ordering
3. **Multiple Suppressions**: Can apply multiple suppressions without conflicts
4. **Pure Transformations**: Rendering is a pure function that can be tested in isolation
5. **Atomic Operations**: Either all suppressions apply correctly or none do

## I/O Shell Components

The I/O shell handles all side effects and coordinates with the functional core:

### 1. Interactive UI with Functional Reactive Architecture

**Recommended Architecture: Model-View-Update Pattern (Elm Architecture)**

The interactive UI should implement a functional reactive architecture with:
- **Immutable state model** representing the complete UI state
- **Pure update functions** that transform state based on input events  
- **Explicit render cycle** that displays the current state after every update
- **Unidirectional data flow**: Input → Update → Render → Repeat

```cpp
// Immutable state model containing ALL UI state
struct UIModel {
    std::vector<Warning> warnings;
    std::vector<Warning> filtered_warnings;
    int current_index = 0;
    NolintStyle current_style = NolintStyle::NONE;
    std::string current_filter;
    std::unordered_map<std::string, NolintStyle> warning_decisions;
    std::unordered_set<std::string> visited_warnings;
    bool in_statistics_mode = false;
    int current_stats_index = 0;
    std::vector<WarningTypeStats> warning_stats;
};

// Input events (all possible user actions)
enum class InputEvent {
    ARROW_UP, ARROW_DOWN,           // Style cycling
    ARROW_LEFT, ARROW_RIGHT,       // Navigation  
    SAVE_EXIT, QUIT,               // Exit actions
    SEARCH, SHOW_STATISTICS,       // Mode changes
    ESCAPE, ENTER                  // Mode navigation
};

// Pure state transition function
auto update(const UIModel& current, const InputEvent& event) -> UIModel {
    UIModel next = current;  // Copy current state
    
    switch (event) {
        case InputEvent::ARROW_UP:
            next.current_style = cycle_style_up(current.current_style);
            break;
        case InputEvent::ARROW_RIGHT:
            if (current.current_index < static_cast<int>(get_active_warnings(current).size()) - 1) {
                next.current_index++;
            }
            break;
        // ... other pure state transitions
    }
    
    return next;  // Return new immutable state
}

// Explicit render function (no hidden state changes)
auto render(const UIModel& model) -> void {
    display_warning_context(model);
    display_status_line(model);
    display_controls();
}

// Simple main loop - no nested loops!
class NolintApp {
public:
    auto run_interactive() -> void {
        UIModel model = initialize_model();
        
        while (true) {
            render(model);                           // Explicit render
            auto input = terminal_->get_input();     // Get input
            model = update(model, input);            // Pure state update
            
            if (should_exit(model)) break;
        }
    }
};
```

**Benefits of This Architecture:**
- **No hidden state mutations** - all state changes explicit and traceable
- **No complex nested loops** - simple Input → Update → Render cycle
- **Predictable behavior** - same input always produces same state transition
- **Easy testing** - pure update functions can be tested in isolation
- **Clear separation** - rendering separate from state management

### 2. Terminal I/O (`terminal_io.hpp`) - Adapted for Functional Reactive UI

```cpp
class Terminal : public ITerminal {
private:
    FILE* tty_file_ = nullptr;          // /dev/tty for user interaction
    bool use_tty_ = false;              // Piped input support
    struct termios original_termios_;   // RAII terminal restoration
    
public:
    // Simplified input for functional reactive pattern
    auto get_input_event() -> InputEvent override;  // Maps keys to events
    auto display_model(const UIModel& model) -> void override;  // Pure rendering
    auto is_interactive() -> bool override;    // Capability detection
};
```

**Responsibilities for Functional Reactive Architecture**:
- **Input abstraction**: Convert raw key presses to semantic `InputEvent`s
- **Pure rendering**: Display UI state without modifying it  
- **Terminal management**: Handle raw mode and restoration (RAII)
- **Event mapping**: Translate terminal codes to application events

**Key Design Change**: Instead of complex terminal state management mixed with UI logic, the terminal becomes a pure input/output adapter that works with the Model-View-Update cycle.

### 3. File System (`file_io.hpp`) - AnnotatedFile Integration
```cpp
class FileSystem : public IFileSystem {
public:
    auto read_file_to_annotated(const std::string& path) -> AnnotatedFile override;
    auto write_annotated_file(const AnnotatedFile& file, const std::string& path) -> bool override;
private:
    auto write_lines_atomic(const std::vector<std::string>& lines, const std::string& path) -> bool;
};
```

**Responsibilities**:
- Convert between disk files and AnnotatedFile structures
- Atomic file operations using render_annotated_file()
- Line ending preservation
- Error handling for inaccessible files
- Ensures suppression ordering is maintained on disk

### 4. Warning Parser (`warning_parser.hpp`)
```cpp
class WarningParser : public IWarningParser {
public:
    auto parse_warnings(std::istream& input) -> std::vector<Warning> override;
};
```

**Responsibilities**:
- Parse clang-tidy output format
- Extract structured warning data
- Handle malformed input gracefully

## Key Architectural Decisions

### 1. Deferred Modifications
Instead of applying changes immediately, all decisions are tracked in memory and applied atomically:

```cpp
// Decision tracking
std::unordered_map<std::string, NolintStyle> warning_decisions_;

// Choice memory when navigating backwards
auto warning_key = get_warning_key(warning);
auto prev_decision = warning_decisions_.find(warning_key);
if (prev_decision != warning_decisions_.end()) {
    current_style = prev_decision->second;  // Restore choice
}
```

**Benefits**:
- Users can navigate freely and change previous decisions
- Atomic saves prevent partial modifications
- Easy rollback if user quits without saving

### 2. Dependency Injection
All major components use interfaces for clean separation and testability:

```cpp
class NolintApp {
    NolintApp(std::unique_ptr<IWarningParser> parser,
              std::unique_ptr<IFileSystem> filesystem,
              std::unique_ptr<ITerminal> terminal);
};
```

**Benefits**:
- Easy unit testing with mock implementations
- Clean separation of concerns
- Flexible component replacement

### 3. RAII Resource Management
Critical resources are managed with RAII patterns:

```cpp
class Terminal {
    ~Terminal() {
        // Automatically restore terminal state
        if (termios_saved_) {
            tcsetattr(fileno(tty_file_), TCSAFLUSH, &original_termios_);
        }
    }
};
```

**Benefits**:
- Automatic cleanup on any exit path
- Exception-safe resource management
- No terminal corruption even on crashes

## Data Flow with Functional Reactive Architecture

### 1. Initialization
```
clang-tidy output → WarningParser → std::vector<Warning> → UIModel (immutable)
```

### 2. Interactive Loop (Model-View-Update Cycle)
```
UIModel → render(model) → Display Current State
    ↑                           ↓
    |                    User Input (keys)
    |                           ↓
    |              get_input_event() → InputEvent
    |                           ↓
    ←─── update(model, event) ── Pure State Transformation
         (returns new UIModel)
```

### 3. State Transitions (Pure Functions)
```
Current UIModel + InputEvent → update() → New UIModel
                                   ↓
              functional_core transformations (if needed)
                                   ↓
                          Updated immutable state
```

### 4. Final Application (on exit)
```
Final Decisions → apply_to_annotated_files() → render_annotated_file() → FileSystem
```

**Critical Edge Case Handling:**
```
Warning A: NOLINT_BLOCK (lines 10-20) + Warning B: NOLINTNEXTLINE (line 10)
                                    ↓
apply_decision() maintains proper order:
1. NOLINTBEGIN (from Warning A) - block starts
2. NOLINTNEXTLINE (from Warning B) - specific line suppression  
3. Target code line 10
4. ... lines 11-19 ...
5. NOLINTEND (from Warning A) - block ends at line 20
```

**Key Benefits of This Flow:**
- **Single source of truth**: All state in UIModel
- **Predictable updates**: Same input always produces same state change
- **Easy debugging**: State transitions are pure and traceable
- **No race conditions**: Immutable state prevents concurrent modification issues

## Testing Strategy

The functional architecture enables a highly effective testing approach:

### 80/20 Testing Split
- **80% Pure Function Tests**: Fast, no mocking, comprehensive coverage
- **20% Integration Tests**: I/O edge cases, error handling

### Test Categories (82 Total Tests)
- **34 Functional Core Tests**: Pure text transformation logic
- **25 Warning Parser Tests**: clang-tidy output parsing
- **11 Filter/Search Tests**: Warning filtering and search functionality
- **11 UI Integration Tests**: Interactive mode with regression coverage
- **1 Real Output Test**: 242 actual clang-tidy warnings

### Example Test Comparison

**Pure Function Test (Simple)**:
```cpp
TEST(FunctionalTest, ApplyNolintBlock) {
    auto result = apply_modification_to_lines(original_lines, modification);
    EXPECT_EQ(result.lines[5], "    // NOLINTBEGIN(readability-function-size)");
    EXPECT_EQ(result.lines[10], "    // NOLINTEND(readability-function-size)");
}
```

**Integration Test (Complex)**:
```cpp
TEST_F(UIIntegrationTest, SearchReducesArrayBounds) {
    // Test comprehensive crash regression with filtered warnings
    auto mock_terminal = std::make_unique<MockTerminal>();
    // ... extensive setup ...
}
```

## Modern C++20 Features

The implementation leverages modern C++20 features throughout:

### Concepts and Constraints
```cpp
template<typename T>
concept WarningLike = requires(T t) {
    { t.file_path } -> std::convertible_to<std::string_view>;
    { t.line_number } -> std::convertible_to<int>;
};
```

### Ranges and Algorithms
```cpp
auto filtered_warnings = warnings 
    | std::views::filter([](const auto& w) { return w.is_valid(); })
    | std::views::transform([](const auto& w) { return process_warning(w); });
```

### Designated Initializers
```cpp
Warning warning{
    .file_path = "/test.cpp",
    .line_number = 42,
    .warning_type = "readability-magic-numbers"
};
```

### RAII and Smart Pointers
```cpp
std::unique_ptr<ITerminal> terminal_;  // Automatic cleanup
std::span<const std::string> lines;   // Safe array access
```

## Performance Characteristics

### Memory Usage
- **Efficient**: Files loaded only when needed
- **Bounded**: File cache with reasonable limits
- **Safe**: Comprehensive bounds checking prevents crashes

### Execution Speed
- **Fast Tests**: Pure functions test in microseconds
- **Responsive UI**: Single-key input with immediate feedback
- **Efficient I/O**: Batched file operations, atomic writes

### Scalability
- **Large Files**: In-memory processing handles thousands of lines
- **Many Warnings**: Efficient filtering and search
- **Memory Safe**: Robust bounds checking prevents crashes

## Error Handling Strategy

### Graceful Degradation
- **Missing Files**: Skip and continue processing
- **Malformed Input**: Parse what's possible, skip invalid lines
- **Terminal Issues**: Fall back to non-interactive mode

### User Feedback
- **Clear Messages**: Informative error messages without technical details
- **Recovery Options**: Allow retry or alternative approaches
- **Status Indicators**: Always show current state and options

### Robustness
- **Crash Prevention**: Comprehensive bounds checking and memory safety
- **State Restoration**: Terminal always restored regardless of exit method
- **Data Protection**: Atomic operations prevent partial modifications

## Key Implementation Insights

### 1. Separation of Concerns Works
The functional core handles all business logic without knowing about files, terminals, or user interaction. The I/O shell coordinates everything without implementing business rules.

### 2. Pure Functions Enable Confident Changes
With 34 pure function tests covering all text transformation logic, refactoring and enhancement can be done with high confidence.

### 3. RAII Prevents Subtle Bugs
Terminal corruption and resource leaks are eliminated through automatic cleanup, even in error conditions.

### 4. Choice Memory is Essential for UX
Users frequently navigate backwards to change decisions. The deferred modification system makes this seamless.

### 5. Real-time Preview Drives Adoption
Seeing exactly how code will change before committing builds user confidence and prevents mistakes.

## Future Architecture Considerations

The current architecture is production-ready and well-tested. Potential enhancements would maintain the functional core pattern:

### Optional Enhancements
- **Configuration System**: Pure functions for config parsing and validation
- **Session State**: Functional approach to progress persistence
- **Plugin System**: Interface-based extensions maintaining pure core
- **Performance Optimization**: Parallel processing of independent transformations

### Architecture Stability
The functional core design provides a stable foundation that can accommodate new features without major restructuring. New functionality can be added as:
- Additional pure functions in the functional core
- New interfaces in the I/O shell
- Enhanced coordination in NolintApp

This architectural pattern has proven effective for complex interactive text processing applications and provides an excellent foundation for long-term maintenance and enhancement.
