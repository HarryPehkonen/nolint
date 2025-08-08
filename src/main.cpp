#include "nolint/application/nolint_app.hpp"
#include "nolint/io/file_system.hpp"
#include "nolint/parsers/warning_parser.hpp"
#include "nolint/ui/terminal.hpp"
#include <iostream>
#include <memory>

auto parse_arguments(int argc, char* argv[]) -> nolint::Config {
    nolint::Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--input" || arg == "-i") {
            if (i + 1 < argc) {
                config.input_file = argv[++i];
            } else {
                std::cerr << "Error: --input requires a filename\n";
                std::exit(1);
            }
        } else if (arg == "--non-interactive" || arg == "-n") {
            config.interactive = false;
        } else if (arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "--load-session") {
            if (i + 1 < argc) {
                config.load_session_file = argv[++i];
            } else {
                std::cerr << "Error: --load-session requires a filename\n";
                std::exit(1);
            }
        } else if (arg == "--save-session") {
            if (i + 1 < argc) {
                config.save_session_file = argv[++i];
            } else {
                std::cerr << "Error: --save-session requires a filename\n";
                std::exit(1);
            }
        } else if (arg == "--default-style") {
            if (i + 1 < argc) {
                std::string style = argv[++i];
                if (style == "nolint") {
                    config.default_style = nolint::NolintStyle::NOLINT_SPECIFIC;
                } else if (style == "nolintnextline") {
                    config.default_style = nolint::NolintStyle::NOLINTNEXTLINE;
                } else if (style == "nolint-block") {
                    config.default_style = nolint::NolintStyle::NOLINT_BLOCK;
                } else {
                    std::cerr << "Error: Invalid style '" << style << "'. Valid options: nolint, nolintnextline, nolint-block\n";
                    std::exit(1);
                }
            } else {
                std::cerr << "Error: --default-style requires a style name\n";
                std::exit(1);
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: nolint [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -i, --input <file>      Read warnings from file (default: stdin)\n";
            std::cout << "  -n, --non-interactive   Apply default style without prompting\n";
            std::cout
                << "      --dry-run           Show what would be changed without modifying files\n";
            std::cout << "      --load-session <file> Load previous decisions from file\n";
            std::cout << "      --save-session <file> Save decisions to file for later use\n";
            std::cout << "      --default-style <style> Set default style for batch mode\n";
            std::cout << "                              (nolint, nolintnextline, nolint-block)\n";
            std::cout << "  -h, --help              Show this help message\n\n";
            std::cout << "Examples:\n";
            std::cout << "  clang-tidy src/*.cpp | nolint\n";
            std::cout << "  nolint --input warnings.txt\n";
            std::cout << "  nolint --input warnings.txt --dry-run\n";
            std::cout << "  nolint --input warnings.txt --save-session session.txt\n";
            std::cout << "  nolint --input warnings.txt --load-session session.txt\n";
            std::cout << "  nolint --input warnings.txt --non-interactive --default-style nolint\n";
            std::exit(0);
        } else {
            std::cerr << "Error: Unknown option " << arg << "\n";
            std::exit(1);
        }
    }

    return config;
}

auto main(int argc, char* argv[]) -> int {
    try {
        auto config = parse_arguments(argc, argv);

        // Create dependencies with dependency injection
        auto terminal = std::make_unique<nolint::Terminal>();
        auto filesystem = std::make_unique<nolint::FileSystem>();
        auto parser = std::make_unique<nolint::WarningParser>();

        // Create and run application
        nolint::NolintApp app(std::move(terminal), std::move(filesystem), std::move(parser));

        return app.run(config);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
}