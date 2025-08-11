#include "file_modifier.hpp"
#include "annotated_file.hpp"
#include <filesystem>
#include <iostream>

namespace nolint {

auto FileModifier::apply_decisions(const std::vector<Warning>& warnings,
                                   const std::unordered_map<size_t, NolintStyle>& decisions,
                                   bool dry_run) -> ModificationResult {
    ModificationResult result;
    result.success = true;

    // Group warnings by file
    auto grouped = group_warnings_by_file(warnings, decisions);

    for (const auto& [file_path, file_warnings] : grouped) {
        try {
            // Load the file into AnnotatedFile
            auto annotated_file = load_annotated_file(file_path);

            // Apply all decisions for this file
            for (const auto& [warning, style] : file_warnings) {
                annotated_file = apply_decision(annotated_file, warning, style);
            }

            if (dry_run) {
                // Just track that we would modify this file
                result.modified_files.push_back(file_path);
                std::cout << "DRY RUN: Would modify " << file_path << "\n";

                // Show preview of changes
                auto rendered = render_annotated_file(annotated_file);
                std::cout << "Preview of " << file_path << ":\n";
                for (size_t i = 0; i < std::min(rendered.size(), size_t(10)); ++i) {
                    std::cout << "  " << (i + 1) << ": " << rendered[i] << "\n";
                }
                if (rendered.size() > 10) {
                    std::cout << "  ... (" << (rendered.size() - 10) << " more lines)\n";
                }
                std::cout << "\n";
            } else {
                // Actually save the file
                if (save_annotated_file(annotated_file, file_path)) {
                    result.modified_files.push_back(file_path);
                    std::cout << "Modified: " << file_path << "\n";
                } else {
                    result.failed_files.push_back(file_path);
                    result.success = false;
                    std::cerr << "Failed to save: " << file_path << "\n";
                }
            }
        } catch (const std::exception& e) {
            result.failed_files.push_back(file_path);
            result.success = false;
            result.error_message = "Error processing " + file_path + ": " + e.what();
        }
    }

    return result;
}

auto FileModifier::preview_file_changes(const std::string& file_path,
                                        const std::vector<Warning>& warnings,
                                        const std::unordered_map<size_t, NolintStyle>& decisions)
    -> std::vector<std::string> {
    try {
        auto annotated_file = load_annotated_file(file_path);

        // Apply relevant decisions
        for (size_t i = 0; i < warnings.size(); ++i) {
            const auto& warning = warnings[i];
            if (warning.file_path == file_path) {
                auto decision_it = decisions.find(i);
                if (decision_it != decisions.end() && decision_it->second != NolintStyle::NONE) {
                    annotated_file = apply_decision(annotated_file, warning, decision_it->second);
                }
            }
        }

        return render_annotated_file(annotated_file);
    } catch (const std::exception&) {
        return {"Error: Could not preview file changes"};
    }
}

auto FileModifier::group_warnings_by_file(const std::vector<Warning>& warnings,
                                          const std::unordered_map<size_t, NolintStyle>& decisions)
    -> std::unordered_map<std::string, std::vector<std::pair<Warning, NolintStyle>>> {
    std::unordered_map<std::string, std::vector<std::pair<Warning, NolintStyle>>> grouped;

    for (size_t i = 0; i < warnings.size(); ++i) {
        auto decision_it = decisions.find(i);
        if (decision_it != decisions.end() && decision_it->second != NolintStyle::NONE) {
            const auto& warning = warnings[i];
            grouped[warning.file_path].emplace_back(warning, decision_it->second);
        }
    }

    return grouped;
}

} // namespace nolint
