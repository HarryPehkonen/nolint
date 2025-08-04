#pragma once

#include "types.hpp"
#include <vector>

namespace nolint {

// Builds context for displaying warnings
class ContextBuilder {
public:
    static constexpr int DEFAULT_CONTEXT_SIZE = 5;
    
    // Build context for a warning
    auto build_context(const Warning& warning, 
                      const std::vector<std::string>& file_lines,
                      NolintStyle style = NolintStyle::NOLINT_SPECIFIC,
                      int context_size = DEFAULT_CONTEXT_SIZE) -> WarningContext;

private:
    auto extract_context_lines(const std::vector<std::string>& file_lines,
                              int warning_line,
                              int context_size) -> std::vector<CodeLine>;
};

} // namespace nolint