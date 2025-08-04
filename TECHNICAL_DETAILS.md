# NOLINT - Technical Details

## Architecture Overview

`nolint` uses an in-memory transformation approach with deferred file writing to maintain line number consistency while processing multiple warnings per file. The architecture emphasizes testability through dependency injection and separation of concerns.

## Minimal Viable Product (MVP)

The MVP focuses on core functionality with simple implementation:

### MVP Features
1. **Parse clang-tidy output** from file or stdin
2. **Display warnings** with 5 lines of context (before/after)
3. **Single NOLINT format**: `// NOLINT(warning-type)` on the same line
4. **Basic interactions**: Y/N/Q/X only (no arrow keys)
5. **In-memory file modification** with line tracking
6. **Batch write** on exit (X command)
7. **Simple std::cout based UI** (no ncurses)

### MVP Limitations
- No NOLINTNEXTLINE or NOLINTBEGIN/END support
- No arrow key navigation between formats
- No color output
- No backup files
- No progress saving/resume
- Fixed context size (5 lines before/after)

### MVP Implementation Priority
1. Warning parser (regex-based)
2. File manager with line offset tracking
3. Simple UI with std::cout
4. Basic NOLINT formatter
5. Main loop connecting components

This MVP can process a clang-tidy output file and add basic NOLINT comments, establishing the core workflow.

## Core Components

### 1. Warning Parser (`WarningParser`)
Parses clang-tidy output to extract structured warning information.

```cpp
struct Warning {
    std::string file_path;
    int line_number;
    int column_number;
    std::string warning_type;      // e.g., "readability-magic-numbers"
    std::string message;
    std::optional<int> function_lines;  // For function-size warnings
};
```

**Parsing Strategy:**
- Line-by-line processing of clang-tidy output
- Regex patterns to extract:
  - Main warning: `^(.+):(\d+):(\d+): warning: (.+) \[(.+)\]$`
  - Function size note: `^.+: note: (\d+) lines including.+$`
- Maintain state between lines to associate notes with warnings

### 2. File Manager (`FileManager`)
Manages file content and modifications in memory.

```cpp
class FileManager {
    struct FileData {
        std::vector<std::string> lines;
        std::vector<std::string> original_lines;  // For rollback
        int line_offset = 0;  // Tracks added lines
        bool modified = false;
    };
    
    std::unordered_map<std::string, FileData> files_;
    
public:
    void load_file(const std::string& path);
    void apply_modification(const std::string& path, const Modification& mod);
    void write_pending_changes();
    void discard_changes();
};
```

### 3. NOLINT Formatter (`NolintFormatter`)
Generates appropriate NOLINT comments with proper formatting.

```cpp
enum class NolintStyle {
    NOLINT,              // // NOLINT
    NOLINT_SPECIFIC,     // // NOLINT(warning-type)
    NOLINTNEXTLINE,      // // NOLINTNEXTLINE(warning-type)
    NOLINT_BLOCK         // // NOLINTBEGIN/END(warning-type)
};

struct Modification {
    int start_line;      // Adjusted for current offset
    int end_line;        // For block suppressions
    NolintStyle style;
    std::string warning_type;
    std::string indent;  // Matches surrounding code
};
```

### 4. Interactive UI (`InteractiveUI`)
Handles user interaction and display using portable terminal I/O.

```cpp
class InteractiveUI {
    NolintStyle current_style_ = NolintStyle::NOLINT_SPECIFIC;
    
public:
    enum class UserAction {
        ACCEPT,      // Y
        SKIP,        // N
        QUIT,        // Q (discard changes)
        EXIT,        // X (save and exit)
        SAVE,        // S (save current file)
        STYLE_UP,    // ↑
        STYLE_DOWN   // ↓
    };
    
    UserAction show_warning_context(const Warning& warning, 
                                   const std::vector<std::string>& lines,
                                   const Modification& proposed_mod);
};

// Portable terminal handling
class TerminalIO {
    bool use_color_ = true;
    
public:
    void setup_raw_mode();      // Platform-specific setup
    void restore_terminal();    // Restore original settings
    char get_key();            // Non-blocking key input
    void clear_line();         // Clear current line
    void move_cursor_up(int n);
    
    // Color support (ANSI escape codes)
    std::string color_warning(const std::string& text);
    std::string color_context(const std::string& text);
    std::string color_highlight(const std::string& text);
};
```

## Implementation Details

### Line Number Management

**Problem:** Adding NOLINT comments changes line numbers for subsequent warnings.

**Solution:** Track line offset per file:
```cpp
// When adding inline comment (// NOLINT):
file_data.line_offset += 0;  // No new lines

// When adding NOLINTNEXTLINE:
file_data.line_offset += 1;  // One new line added

// When adding NOLINTBEGIN/END:
file_data.line_offset += 2;  // Two new lines added

// Adjust warning line for current offset:
int actual_line = warning.line_number + file_data.line_offset;
```

### Finding Function Boundaries

For function-level warnings with size information:

```cpp
std::pair<int, int> find_function_bounds(
    const std::vector<std::string>& lines,
    int warning_line,
    int function_size) {
    
    // Start: Search backwards for function signature
    // Look for patterns like "type function_name(" not indented
    int start = warning_line;
    for (int i = warning_line - 1; i >= 0; --i) {
        if (is_function_signature(lines[i])) {
            start = i;
            break;
        }
    }
    
    // End: Use the function size hint
    int end = start + function_size - 1;
    
    // Validate with brace matching as safety check
    return {start, end};
}
```

### Indentation Matching

```cpp
std::string extract_indent(const std::string& line) {
    size_t first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return "";
    }
    return line.substr(0, first_non_space);
}
```

### Context Display

```cpp
void display_context(const std::vector<std::string>& lines,
                    int warning_line,
                    int before_context,
                    int after_context,
                    const Modification& mod) {
    
    // For NOLINT_BLOCK, show extended context
    if (mod.style == NolintStyle::NOLINT_BLOCK) {
        before_context = warning_line - mod.start_line + 5;
        after_context = mod.end_line - warning_line + 5;
    }
    
    // Display with highlighting and proposed changes
    for (int i = start; i <= end; ++i) {
        if (i == mod.start_line && mod.style == NolintStyle::NOLINT_BLOCK) {
            // Show where NOLINTBEGIN would go
        }
        // ... display logic
    }
}
```

## Workflow

1. **Initialization**
   - Parse command-line arguments
   - Set up input stream (file or stdin)

2. **Warning Processing Loop**
   ```cpp
   for (const auto& warning : parse_warnings(input)) {
       // Load file if not already loaded
       file_manager.load_file_if_needed(warning.file_path);
       
       // Calculate modification with current style
       auto mod = create_modification(warning, current_style);
       
       // Show context and get user action
       auto action = ui.show_warning_context(warning, 
                                            file_manager.get_lines(warning.file_path),
                                            mod);
       
       switch (action) {
           case ACCEPT:
               file_manager.apply_modification(warning.file_path, mod);
               break;
           case SAVE:
               file_manager.write_file(warning.file_path);
               break;
           // ... other actions
       }
   }
   ```

3. **Finalization**
   - On EXIT: Write all pending changes
   - On QUIT: Discard all changes
   - Show summary of modifications

## Line Ending Handling

```cpp
enum class LineEnding {
    LF,     // Unix/Linux/macOS
    CRLF,   // Windows
    AUTO    // Detect from file
};

class FileManager {
    LineEnding detect_line_ending(const std::string& content) {
        size_t crlf_count = 0;
        size_t lf_count = 0;
        
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\n') {
                if (i > 0 && content[i-1] == '\r') {
                    crlf_count++;
                } else {
                    lf_count++;
                }
            }
        }
        
        return (crlf_count > lf_count) ? LineEnding::CRLF : LineEnding::LF;
    }
    
    std::string get_line_ending_string(LineEnding ending) {
        return (ending == LineEnding::CRLF) ? "\r\n" : "\n";
    }
};
```

## Simple Terminal I/O

```cpp
// Simple portable I/O using iostream
class SimpleUI : public IUserInterface {
    bool use_color_ = true;
    
public:
    auto display_context(const WarningContext& context) -> void override {
        // Clear screen or add spacing
        std::cout << "\n\033[2J\033[H"; // ANSI clear screen
        
        // Show progress
        std::cout << "[" << context.current << "/" << context.total << "] "
                  << "Processing " << context.warning.warning_type 
                  << " in " << context.warning.file_path 
                  << ":" << context.warning.line_number << "\n\n";
        
        // Show code context
        for (const auto& line : context.lines) {
            if (line.number == context.warning.line_number) {
                std::cout << " >> " << std::setw(4) << line.number << " | " 
                         << highlight(line.text) << "\n";
            } else {
                std::cout << "    " << std::setw(4) << line.number << " | " 
                         << line.text << "\n";
            }
        }
        
        // Show proposed change
        std::cout << "\nApply NOLINT? Format: " << format_string(context.style) << "\n";
        std::cout << "[Y]es / [N]o / [Q]uit / e[X]it+save / [↑↓] Change format: ";
        std::cout.flush();
    }
    
    auto get_user_action() -> UserAction override {
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) return UserAction::SKIP;
        
        switch (std::tolower(input[0])) {
            case 'y': return UserAction::ACCEPT;
            case 'n': return UserAction::SKIP;
            case 'q': return UserAction::QUIT;
            case 'x': return UserAction::EXIT;
            case 's': return UserAction::SAVE;
            // For MVP, skip arrow key handling
            default: return UserAction::SKIP;
        }
    }
};
```

## Error Handling

- **File Access**: Skip warnings for inaccessible files, report to user
- **Malformed Warnings**: Skip unparseable lines, continue processing
- **Write Failures**: Keep in-memory changes, allow retry or alternative save location
- **Interrupt Handling**: Catch SIGINT, offer to save pending changes
- **Terminal Restore**: Always restore terminal settings on exit (RAII pattern)

## Testability Design

### Dependency Injection
All major components use interfaces for easy mocking:

```cpp
class IWarningParser {
public:
    virtual ~IWarningParser() = default;
    virtual std::vector<Warning> parse(std::istream& input) = 0;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual std::vector<std::string> read_file(const std::string& path) = 0;
    virtual void write_file(const std::string& path, const std::vector<std::string>& lines) = 0;
    virtual bool file_exists(const std::string& path) = 0;
};

class IUserInterface {
public:
    virtual ~IUserInterface() = default;
    virtual UserAction get_user_action() = 0;
    virtual void display_context(const WarningContext& context) = 0;
};
```

### Core Processing Logic
Separate from I/O for unit testing:

```cpp
class NolintProcessor {
    std::unique_ptr<IWarningParser> parser_;
    std::unique_ptr<IFileSystem> file_system_;
    std::unique_ptr<IUserInterface> ui_;
    
public:
    // Fully testable with mock dependencies (trailing return type style)
    NolintProcessor(std::unique_ptr<IWarningParser> parser,
                   std::unique_ptr<IFileSystem> fs,
                   std::unique_ptr<IUserInterface> ui);
    
    auto process_warnings(std::istream& input) -> void;
};

// RAII for file modifications
class FileTransaction {
    FileManager& manager_;
    bool committed_ = false;
    
public:
    explicit FileTransaction(FileManager& manager) : manager_(manager) {}
    
    ~FileTransaction() {
        if (!committed_) {
            manager_.rollback_all();
        }
    }
    
    auto commit() -> void {
        manager_.write_all();
        committed_ = true;
    }
    
    // Delete copy operations
    FileTransaction(const FileTransaction&) = delete;
    auto operator=(const FileTransaction&) -> FileTransaction& = delete;
    
    // Move operations
    FileTransaction(FileTransaction&& other) noexcept 
        : manager_(other.manager_), committed_(other.committed_) {
        other.committed_ = true;  // Prevent rollback in moved-from object
    }
};
```

### Test Utilities

```cpp
// Mock implementations for testing
class MockFileSystem : public IFileSystem {
    std::map<std::string, std::vector<std::string>> files_;
public:
    void add_file(const std::string& path, const std::vector<std::string>& content);
    // Track all file operations for verification
};

class MockUI : public IUserInterface {
    std::queue<UserAction> scripted_actions_;
public:
    void queue_action(UserAction action);
    // Verify displayed contexts
};
```

## Build System Integration

### CMakeLists.txt
```cmake
# Format target
add_custom_target(format
    COMMAND clang-format -i ${SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Tidy target
add_custom_target(tidy
    COMMAND clang-tidy ${SOURCE_FILES} -- ${COMPILE_FLAGS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Run format and tidy before building
add_dependencies(nolint format)
```

### Pre-commit Hook
```bash
#!/bin/bash
# .git/hooks/pre-commit
clang-format -i $(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(cpp|hpp|h)$')
clang-tidy $(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(cpp|hpp|h)$')
```

## Performance Considerations

- **Lazy File Loading**: Only load files when first warning encountered
- **Efficient String Storage**: Use string views where possible
- **Batch I/O**: Write complete files rather than line-by-line
- **Memory Management**: Clear file data after writing if processing many files

## Testing Strategy

### Unit Tests
- **WarningParser**: Test with various clang-tidy output formats
- **FileManager**: Test modification tracking and line offset calculations
- **NolintFormatter**: Test all NOLINT styles and indentation matching
- **Mock-based tests**: Test core logic without file I/O

### Integration Tests
- Full workflow with real clang-tidy output
- Multi-file scenarios with cross-file line number tracking
- Interrupt handling and recovery

### Test Data
```cpp
// tests/test_data.hpp
namespace test_data {
    const char* SIMPLE_WARNING = R"(
/path/to/file.cpp:42:24: warning: 75 is a magic number [readability-magic-numbers]
)";

    const char* FUNCTION_SIZE_WARNING = R"(
/path/to/file.cpp:100:5: warning: function 'process' exceeds recommended size [readability-function-size]
/path/to/file.cpp:100:5: note: 55 lines including whitespace and comments (threshold 30)
)";
}
```