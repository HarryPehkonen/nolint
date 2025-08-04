# NOLINT - Requirements Document

## Overview
`nolint` is a command-line tool that automatically applies `// NOLINT` comments to C++ source code based on clang-tidy warnings. It provides an interactive interface for reviewing and selectively suppressing linting warnings.

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
  - For NOLINTBEGIN/END mode, show extended context (entire function)
- Present user options:
  - `Y` - Accept and apply suppression comment
  - `N` - Skip this warning
  - `Q` - Quit without saving changes
  - `X` - Exit and save all pending changes
  - `S` - Save current file and continue
  - `↑/↓` - Cycle through different NOLINT formats (re-displays context with new format)

#### 3. Suppression Comment Types
Support multiple NOLINT formats (cycled with arrow keys):
1. `// NOLINT` - Suppress all warnings on the line
2. `// NOLINT(warning-name)` - Suppress specific warning on the line
3. `// NOLINTNEXTLINE(warning-name)` - Suppress warning on the next line
4. `// NOLINTBEGIN(warning-name)` and `// NOLINTEND(warning-name)` - Block suppression

For NOLINTBEGIN/END placement:
- **Function-level warnings** with size notes: Use the reported line count to determine function bounds
- **Other warnings**: Place BEGIN at warning line, END after configurable lines (default: 5)

#### 4. File Modification Strategy
- Load entire file into memory (`std::vector<std::string>`) on first warning
- Apply modifications to in-memory representation
- Track line number offsets as modifications are made
- Preserve original file formatting and indentation
- Write files only when:
  - User selects `X` (exit and save)
  - User selects `S` (save current file)
- Support backup files before modification (optional)

### Command-Line Interface

#### Basic Usage
```bash
# From file
nolint --input warnings.txt

# Pipe mode
clang-tidy src/*.cpp | nolint

# With custom context
nolint --input warnings.txt -B 10 -A 10
```

#### Options
- `--input <file>` - Read warnings from file
- `-B <n>` - Number of lines to show before warning (default: 5)
- `-A <n>` - Number of lines to show after warning (default: 5)
- `--begin-end-span <n>` - Lines between BEGIN/END for non-function warnings (default: 5)
- `--backup` - Create backup files before modification (.bak extension)
- `--filter <pattern>` - Only process warnings matching pattern
- `--exclude <pattern>` - Skip warnings matching pattern
- `--color` / `--no-color` - Enable/disable colored output
- `--summary` - Show summary of changes at the end
- `--line-ending <auto|lf|crlf>` - Line ending style (default: auto-detect)
- `--preserve-line-endings` - Maintain original line endings (default behavior)

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
$ clang-tidy src/*.cpp | nolint -B 3 -A 3

[1/15] Processing readability-magic-numbers in src/main.cpp:42

    39 | void process_data() {
    40 |     const int iterations = 100;
    41 |     for (int i = 0; i < iterations; ++i) {
 >> 42 |         if (data[i] > 75) {  // warning: 75 is a magic number
    43 |             handle_threshold(data[i]);
    44 |         }
    45 |     }

Apply NOLINT? Format: // NOLINT(readability-magic-numbers)
[Y]es / [N]o / [Q]uit / e[X]it+save / [S]ave file / [↑↓] Change format: _

> Y

[2/15] Processing readability-function-size in src/parser.cpp:78

    75 | // Complex parsing function
    76 | // TODO: Consider refactoring
    77 |
 >> 78 | void Parser::parse_expression(const Token* tokens, size_t count) {
    79 |     // note: 44 lines including whitespace and comments (threshold 30)
    ...
   122 | }

Apply NOLINT? Format: // NOLINTBEGIN(readability-function-size) ... END
[Y]es / [N]o / [Q]uit / e[X]it+save / [S]ave file / [↑↓] Change format: _

> X

Saving changes...
✓ src/main.cpp (1 modification)
✓ src/parser.cpp (1 modification)

2 files modified, 2 warnings suppressed.
```

## Future Enhancements
- Configuration file support (`.nolintrc`)
- Undo last change functionality
- Smart function detection using clang AST
- Integration with version control (show diff before saving)
- Batch mode for CI/CD pipelines
- Statistics export (CSV/JSON format)