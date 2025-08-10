#include "ui_model.hpp"

namespace nolint {

// Pure state transition function - no side effects!
auto update(UIModel model, InputEvent event) -> UIModel {
    // Early return if no warnings
    if (model.warnings.empty()) {
        if (event == InputEvent::QUIT) {
            model.should_exit = true;
        }
        return model;
    }
    
    switch (event) {
        case InputEvent::QUIT:
            model.should_exit = true;
            break;
            
        case InputEvent::ARROW_LEFT:
            // Navigate to previous warning
            if (model.current_index > 0) {
                model.current_index--;
            }
            break;
            
        case InputEvent::ARROW_RIGHT:
            // Navigate to next warning
            if (model.current_index < model.warnings.size() - 1) {
                model.current_index++;
            }
            break;
            
        case InputEvent::ARROW_UP:
            // Cycle suppression style forward
            {
                auto current = static_cast<int>(model.current_style());
                current = (current + 1) % 4;
                model.decisions[model.current_index] = static_cast<NolintStyle>(current);
                
                // Track that this file will be modified
                if (model.current_style() != NolintStyle::NONE) {
                    model.modified_files.insert(model.current_warning().file_path);
                }
            }
            break;
            
        case InputEvent::ARROW_DOWN:
            // Cycle suppression style backward
            {
                auto current = static_cast<int>(model.current_style());
                current = (current + 3) % 4;  // +3 is same as -1 mod 4
                model.decisions[model.current_index] = static_cast<NolintStyle>(current);
                
                // Track that this file will be modified
                if (model.current_style() != NolintStyle::NONE) {
                    model.modified_files.insert(model.current_warning().file_path);
                }
            }
            break;
            
        case InputEvent::SHOW_STATISTICS:
            model.show_statistics = !model.show_statistics;
            break;
            
        case InputEvent::SAVE_EXIT:
            // TODO: Mark for save
            model.should_exit = true;
            break;
            
        case InputEvent::SEARCH:
            // TODO: Enter search mode
            break;
            
        case InputEvent::UNKNOWN:
        default:
            // No state change
            break;
    }
    
    return model;
}

} // namespace nolint
