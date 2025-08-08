#include "nolint/ui/ui_model.hpp"
#include "nolint/core/warning.hpp"
#include <fstream>
#include <sstream>

namespace nolint {

auto style_to_string(NolintStyle style) -> std::string {
    switch (style) {
    case NolintStyle::NONE:
        return "NONE";
    case NolintStyle::NOLINT_SPECIFIC:
        return "NOLINT_SPECIFIC";
    case NolintStyle::NOLINTNEXTLINE:
        return "NOLINTNEXTLINE";
    case NolintStyle::NOLINT_BLOCK:
        return "NOLINT_BLOCK";
    }
    return "NONE";
}

auto string_to_style(const std::string& str) -> NolintStyle {
    if (str == "NOLINT_SPECIFIC") return NolintStyle::NOLINT_SPECIFIC;
    if (str == "NOLINTNEXTLINE") return NolintStyle::NOLINTNEXTLINE;
    if (str == "NOLINT_BLOCK") return NolintStyle::NOLINT_BLOCK;
    return NolintStyle::NONE;
}

auto save_decisions(const Decisions& decisions, const std::string& file_path) -> bool {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    // Simple format: one decision per line
    // Format: warning_key|style
    for (const auto& [key, style] : decisions) {
        if (style != NolintStyle::NONE) {  // Only save non-NONE decisions
            file << key << "|" << style_to_string(style) << "\n";
        }
    }

    return file.good();
}

auto load_decisions(const std::string& file_path) -> std::optional<Decisions> {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    Decisions decisions;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        // Parse: warning_key|style (exactly one pipe)
        auto pipe_pos = line.find('|');
        if (pipe_pos == std::string::npos) continue;
        
        // Check that there's only one pipe
        if (line.find('|', pipe_pos + 1) != std::string::npos) continue;
        
        std::string key = line.substr(0, pipe_pos);
        std::string style_str = line.substr(pipe_pos + 1);
        
        auto style = string_to_style(style_str);
        decisions[key] = style;
    }

    return decisions;
}

} // namespace nolint