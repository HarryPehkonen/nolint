# nolint

A C++20 command-line tool that automates adding `// NOLINT` comments to C++ source code based on clang-tidy warnings. Features an interactive interface for reviewing and selectively suppressing linting warnings.

## Features

- **Interactive Preview**: Shows actual code with NOLINT comments applied before modification
- **Multiple Suppression Styles**: 
  - `// NOLINT(warning-type)` - Inline suppression
  - `// NOLINTNEXTLINE(warning-type)` - Previous line suppression  
  - `// NOLINTBEGIN/NOLINTEND(warning-type)` - Block suppression for functions
- **Piped Input Support**: Works with `clang-tidy output.txt | nolint` while maintaining interactivity
- **Smart Navigation**: Forward/backward through warnings with choice memory
- **Color-coded Display**: Green highlighting for NOLINT comments in preview
- **Single-key Controls**: No Enter key required for navigation
- **Atomic File Operations**: Safe modification with proper error handling

## Quick Start

```bash
# Build
mkdir build && cd build
cmake -DCMAKE_CXX_STANDARD=20 ..
make -j$(nproc)

# Interactive mode with clang-tidy output
clang-tidy src/*.cpp -- -std=c++20 > warnings.txt
./nolint --input warnings.txt

# Piped input (maintains interactivity via /dev/tty)
clang-tidy src/*.cpp -- -std=c++20 | ./nolint

# Non-interactive mode
./nolint --input warnings.txt --non-interactive --default-style nolintnextline
```

## Interactive Controls

- **↑/↓ Arrow Keys**: Cycle through suppression styles
- **s**: Accept current style and continue to next warning
- **k**: Skip this warning (no suppression)
- **p**: Go to previous warning (remembers your choice)
- **x**: Save all changes and exit
- **q**: Quit without saving changes

## Requirements

- C++20 compiler (GCC 10+, Clang 12+)
- CMake 3.20+
- Google Test (for building tests)
- clang-tidy (for generating warnings)

## Architecture

Built with modern C++20 features and functional programming principles:
- **Pure Functions**: Core text transformations are side-effect free and extensively tested
- **Dependency Injection**: All I/O operations are abstracted for testability
- **RAII**: Proper resource management with automatic cleanup
- **Concepts & Ranges**: Type-safe generic programming with readable algorithms
