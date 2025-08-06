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
    // Text transformation - core business logic
    auto apply_modification_to_lines(
        const std::vector<std::string>& original_lines,
        const Modification& modification
    ) -> TextTransformation;
    
    // Context building for UI display
    auto build_display_context(
        const Warning& warning,
        const std::vector<std::string>& file_lines,
        NolintStyle current_style
    ) -> DisplayContext;
    
    // Warning filtering and search
    auto filter_warnings(
        const std::vector<Warning>& warnings,
        const std::string& filter
    ) -> std::vector<Warning>;
    
    // NOLINT comment creation
    auto create_modification(
        const Warning& warning,
        NolintStyle style,
        const std::vector<std::string>& file_lines
    ) -> Modification;
}
```

### Benefits of Pure Functions

1. **Easy Testing**: No mocking required - just input → output verification
2. **Fast Tests**: No I/O operations, tests run in milliseconds
3. **Deterministic**: Same input always produces same output
4. **Composable**: Functions can be easily combined and reused
5. **Debuggable**: No hidden state or side effects to track

## I/O Shell Components

The I/O shell handles all side effects and coordinates with the functional core:

### 1. NolintApp (Main Orchestrator)
```cpp
class NolintApp {
private:
    std::unique_ptr<IWarningParser> parser_;
    std::unique_ptr<IFileSystem> filesystem_;
    std::unique_ptr<ITerminal> terminal_;
    
    // Decision tracking (deferred modifications)
    std::unordered_map<std::string, NolintStyle> warning_decisions_;
    
public:
    auto run(const AppConfig& config) -> int;
    auto process_warnings(const std::vector<Warning>& warnings) -> void;
};
```

**Responsibilities**:
- Coordinate between all components
- Track user decisions with choice memory
- Manage deferred modifications until save
- Handle user navigation and interaction flow

### 2. Terminal I/O (`terminal_io.hpp`)
```cpp
class Terminal : public ITerminal {
private:
    FILE* tty_file_ = nullptr;          // /dev/tty for user interaction
    bool use_tty_ = false;              // Piped input support
    struct termios original_termios_;   // RAII terminal restoration
    
public:
    auto read_char() -> char override;  // Single-key input
    auto read_line() -> std::string override;  // Search input with echo
    auto is_interactive() -> bool override;    // Capability detection
};
```

**Responsibilities**:
- Handle piped input with `/dev/tty` fallback
- Raw terminal mode for single-key navigation
- Perfect terminal state restoration (RAII)
- Character echoing for search input visibility

### 3. File System (`file_io.hpp`)
```cpp
class FileSystem : public IFileSystem {
public:
    auto read_file(const std::string& path) -> std::vector<std::string> override;
    auto write_file(const std::string& path, std::span<const std::string> lines) -> bool override;
};
```

**Responsibilities**:
- Atomic file operations
- Line ending preservation
- Error handling for inaccessible files

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

## Data Flow

### 1. Input Processing
```
clang-tidy output → WarningParser → std::vector<Warning>
                                          ↓
FileSystem ← NolintApp ← filtered warnings
```

### 2. Interactive Processing
```
Warning + File Lines → functional_core::build_display_context → DisplayContext
                                          ↓
Terminal ← NolintApp ← User Decision (with choice memory)
                                          ↓
functional_core::create_modification → Modification (deferred)
```

### 3. Final Application
```
All Decisions → functional_core::apply_modification_to_lines → Modified Lines
                                          ↓
FileSystem ← Atomic Write Operations
```

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
