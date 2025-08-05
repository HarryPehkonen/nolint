#include "file_io.hpp"
#include "nolint_app.hpp"
#include "terminal_io.hpp"
#include "warning_parser.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Interactive tool for adding NOLINT comments to suppress clang-tidy warnings.\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "  -i, --input FILE     Read clang-tidy output from FILE (default: stdin)\n";
    std::cout << "  -s, --style STYLE    Default suppression style: specific|nextline|block\n";
    std::cout << "                       (default: specific)\n";
    std::cout << "  -n, --non-interactive Apply default style to all warnings without prompting\n";
    std::cout << "  --dry-run            Show what would be changed without modifying files\n";
    std::cout << "  --force              Allow file modifications when input is piped (unsafe)\n";
    std::cout << "  -h, --help           Show this help message\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "  " << program_name << " --input tidy_output.txt\n";
    std::cout << "  clang-tidy src/*.cpp | " << program_name << "\n";
    std::cout << "  " << program_name << " --style nextline --non-interactive < warnings.txt\n";
}

auto parse_style(const std::string& style_str) -> std::optional<nolint::NolintStyle> {
    if (style_str == "specific")
        return nolint::NolintStyle::NOLINT_SPECIFIC;
    if (style_str == "nextline")
        return nolint::NolintStyle::NOLINTNEXTLINE;
    if (style_str == "block")
        return nolint::NolintStyle::NOLINT_BLOCK;
    return std::nullopt;
}

auto parse_args(int argc, char** argv) -> std::optional<nolint::AppConfig> {
    nolint::AppConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return std::nullopt;
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a filename\n";
                return std::nullopt;
            }
            config.input_file = argv[++i];
        } else if (arg == "-s" || arg == "--style") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a style\n";
                return std::nullopt;
            }
            auto style = parse_style(argv[++i]);
            if (!style.has_value()) {
                std::cerr << "Error: Invalid style. Must be: specific, nextline, or block\n";
                return std::nullopt;
            }
            config.default_style = style.value();
        } else if (arg == "-n" || arg == "--non-interactive") {
            config.interactive = false;
        } else if (arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "--force") {
            config.force = true;
        } else {
            std::cerr << "Error: Unknown option " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return std::nullopt;
        }
    }

    return config;
}

} // anonymous namespace

int main(int argc, char** argv) {
    auto config = parse_args(argc, argv);
    if (!config.has_value()) {
        return 1;
    }

    // Create dependencies
    auto parser = std::make_unique<nolint::ClangTidyParser>();
    auto filesystem = std::make_unique<nolint::FileSystem>();
    auto terminal = std::make_unique<nolint::Terminal>();

    // Create and run application
    nolint::NolintApp app(std::move(parser), std::move(filesystem), std::move(terminal));

    return app.run(config.value());
}