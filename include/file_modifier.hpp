#pragma once

#include "ui_model.hpp"
#include "annotated_file.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace nolint {

// Service for applying user decisions to files
class FileModifier {
public:
    struct ModificationResult {
        bool success = false;
        std::string error_message;
        std::vector<std::string> modified_files;
        std::vector<std::string> failed_files;
    };
    
    // Apply all decisions to their respective files
    auto apply_decisions(const std::vector<Warning>& warnings, 
                        const std::unordered_map<size_t, NolintStyle>& decisions,
                        bool dry_run = false) -> ModificationResult;
    
    // Preview what a file would look like after modifications
    auto preview_file_changes(const std::string& file_path,
                             const std::vector<Warning>& warnings,
                             const std::unordered_map<size_t, NolintStyle>& decisions) 
                             -> std::vector<std::string>;

private:
    // Group warnings by file for efficient processing
    auto group_warnings_by_file(const std::vector<Warning>& warnings,
                               const std::unordered_map<size_t, NolintStyle>& decisions) 
                               -> std::unordered_map<std::string, std::vector<std::pair<Warning, NolintStyle>>>;
};

} // namespace nolint