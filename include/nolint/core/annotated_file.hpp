#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nolint {

// AnnotatedFile prevents line number drift by maintaining original structure
struct AnnotatedLine {
    std::string text;                            // Original line content
    std::vector<std::string> before_comments;    // ORDERED: NOLINTBEGIN first, then NOLINTNEXTLINE
    std::optional<std::string> inline_comment;   // Inline NOLINT
};

struct BlockSuppression {
    size_t start_line{};      // Original line number (never changes!)
    size_t end_line{};        // Original line number (never changes!)
    std::string warning_type;
};

struct AnnotatedFile {
    std::vector<AnnotatedLine> lines;           // Original structure preserved
    std::vector<BlockSuppression> blocks;       // NOLINTBEGIN/END pairs
};

// Pure functions for AnnotatedFile manipulation
auto create_annotated_file(const std::vector<std::string>& lines) -> AnnotatedFile;

// Render AnnotatedFile to final text with proper ordering
auto render_annotated_file(const AnnotatedFile& file) -> std::vector<std::string>;

// Extract indentation from a line
auto extract_indentation(const std::string& line) -> std::string;

} // namespace nolint