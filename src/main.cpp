#include "nolint/nolint_processor.hpp"
#include "nolint/warning_parser.hpp"
#include "nolint/file_manager.hpp"
#include "nolint/simple_ui.hpp"
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

using namespace nolint;

auto print_usage(const std::string& program_name) -> void {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --input <file>    Read clang-tidy warnings from file\n";
    std::cout << "  --help           Show this help message\n";
    std::cout << "\nIf no --input is specified, reads from stdin.\n";
    std::cout << "\nExample:\n";
    std::cout << "  clang-tidy src/*.cpp | " << program_name << "\n";
    std::cout << "  " << program_name << " --input warnings.txt\n";
}

auto main(int argc, char* argv[]) -> int {
    std::string input_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--input") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --input requires a filename\n";
                return 1;
            }
            input_file = argv[++i];
        }
        else {
            std::cerr << "Error: Unknown option " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    try {
        // Create components
        auto parser = std::make_unique<WarningParser>();
        auto file_system = std::make_unique<FileSystem>();
        auto file_manager = std::make_unique<FileManager>(std::move(file_system));
        auto ui = std::make_unique<SimpleUI>();
        
        // Create processor
        NolintProcessor processor(std::move(parser), std::move(file_manager), std::move(ui));
        
        // Process warnings
        if (input_file.empty()) {
            // Read from stdin
            std::cout << "Reading clang-tidy warnings from stdin...\n";
            processor.process_warnings(std::cin);
        } else {
            // Read from file
            std::ifstream file(input_file);
            if (!file.is_open()) {
                std::cerr << "Error: Cannot open input file: " << input_file << "\n";
                return 1;
            }
            
            std::cout << "Reading clang-tidy warnings from " << input_file << "...\n";
            processor.process_warnings(file);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}