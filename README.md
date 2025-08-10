# nolint

A C++20 command-line tool that automates adding `// NOLINT` comments to C++ source code based on clang-tidy warnings. Features an interactive interface for reviewing and selectively suppressing linting warnings.

## Key Features

###   **Choice Memory**
Navigate freely between warnings - all your decisions are remembered! Go back to any previous warning and change your mind without losing progress.

###   **Search & Filter** 
Find specific warnings instantly with `/` key. Filter by warning type (`readability-magic-numbers`), file path, or message content. Works seamlessly with choice memory.

###   **Interactive Preview**
See exactly how your code will look with NOLINT comments applied before making changes. Real-time preview updates as you cycle through suppression styles.

###   **Single-Key Navigation**
- **Single-key controls**: No Enter key required - immediate response
- **Arrow key navigation**: `←→` to move between warnings, `↑↓` to change styles
- **Auto-save**: All style changes immediately preserved
- **Bidirectional**: Navigate backwards and forwards seamlessly

###   **Multiple Suppression Styles**
- `// NOLINT(warning-type)` - Inline suppression
- `// NOLINTNEXTLINE(warning-type)` - Previous line suppression  
- `// NOLINTBEGIN/NOLINTEND(warning-type)` - Block suppression for functions
- Smart availability based on warning context

###   **Advanced Capabilities**
- **Piped Input Support**: Works with `clang-tidy output.txt | nolint` while maintaining full interactivity
- **Color-coded Display**: Green highlighting for NOLINT comments in preview
- **Atomic File Operations**: Safe modification with proper error handling
- **Memory-Safe**: Comprehensive bounds checking prevents crashes
- **Terminal-Safe**: State restoration using RAII patterns

## Quick Start

```bash
# Build
cmake -B build
cmake --build build

# GCC shows unrealistic warnings regarding __builtin_memcpy.
# For Release builds, use Clang to avoid false-positive warnings:
CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Interactive mode with clang-tidy output
clang-tidy src/*.cpp -- -std=c++20 > warnings.txt
nolint --input warnings.txt

# Piped input (maintains interactivity via /dev/tty)
clang-tidy src/*.cpp -- -std=c++20 | nolint

# Non-interactive mode
nolint --input warnings.txt --non-interactive --default-style nolintnextline
```

## Interactive Controls

- **↑/↓ Arrow Keys**: Cycle through suppression styles (auto-saved)
- **←/→ Arrow Keys**: Navigate between warnings  
- **x**: Save all changes and exit with summary
- **q**: Quit without saving (with confirmation)
- **/**: Search/filter warnings by type or content

## Requirements

- C++20 compiler (GCC 10+, Clang 12+)
- CMake 3.20+
- Google Test (for building tests)
- clang-tidy (for generating warnings)

## Architecture

Built with a **Functional Core / I/O Shell** design using modern C++20 with **Functional Reactive UI**:

### **Interactive UI: Model-View-Update Pattern**
The interactive interface uses a **functional reactive architecture**:
- **Immutable State Model**: All UI state in a single `UIModel` struct
- **Pure Update Functions**: State transitions are pure functions with no side effects
- **Explicit Render Cycle**: Clear separation between state updates and display
- **Unidirectional Data Flow**: Simple `Input → Update → Render` cycle

```cpp
// All UI state in one immutable struct
struct UIModel {
    std::vector<Warning> warnings;
    int current_index = 0;
    NolintStyle current_style = NolintStyle::NONE;
    std::unordered_map<std::string, NolintStyle> warning_decisions;
    bool should_save_and_exit = false;
};

// Pure state transition function
auto update(const UIModel& current, const InputEvent& event) -> UIModel;

// Simple main loop - no nested loops!
while (!should_exit(model)) {
    render(model);                    // Explicit render
    auto event = get_input();         // Get input event
    model = update(model, event);     // Pure state update
}
```

### **Core Architecture**
- **Functional Core**: Pure text transformations in `functional_core.hpp` - no side effects, extensively tested
- **I/O Shell**: `NolintApp`, `Terminal`, `FileSystem` handle all side effects and coordinate with core
- **Modern C++20**: Concepts, ranges, RAII patterns, and designated initializers throughout  
- **Dependency Injection**: All I/O operations abstracted with interfaces for clean testing

**Benefits**: Eliminates hidden state mutations, complex nested loops, and state synchronization bugs. Makes UI behavior predictable and testable.

See `ARCHITECTURE.md` for detailed design documentation.

## Testing

Comprehensive test suite with 82 passing tests:

```bash
cd build
ctest --output-on-failure
# or directly:
./tests/nolint_tests
```

Test categories:
- **34 Functional Core Tests**: Pure text transformation functions
- **25 Warning Parser Tests**: clang-tidy output parsing
- **11 Filter/Search Tests**: Warning filtering and search functionality  
- **11 UI Integration Tests**: Interactive mode with regression coverage
- **1 Real Output Test**: 242 actual clang-tidy warnings

## Production Status

All major issues resolved:
- Memory-safe with comprehensive bounds checking
- Terminal state restoration
- Search functionality with crash regression tests
- Auto-save for all user interactions
- Exit interface

The tool is functional for managing clang-tidy suppressions.
