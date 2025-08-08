#include "nolint/application/nolint_app.hpp"
#include "nolint/io/file_system.hpp"
#include "nolint/parsers/warning_parser.hpp"
#include "nolint/ui/terminal.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>

namespace nolint {

// Mock implementations for testing batch mode
class MockTerminalBatch : public ITerminal {
public:
    MOCK_METHOD(bool, setup_raw_mode, (), (override));
    MOCK_METHOD(void, restore_terminal_state, (), (override));
    MOCK_METHOD(InputEvent, get_input_event, (), (override));
    MOCK_METHOD(void, display_screen, (const Screen&), (override));
    MOCK_METHOD(std::string, read_line, (), (override));
    MOCK_METHOD(bool, is_interactive, (), (override));
};

class MockFileSystemBatch : public IFileSystem {
public:
    MOCK_METHOD(AnnotatedFile, read_file_to_annotated, (const std::string&), (override));
    MOCK_METHOD(bool, write_annotated_file, (const AnnotatedFile&, const std::string&), (override));
    MOCK_METHOD(bool, file_exists, (const std::string&), (override));
};

class MockWarningParserBatch : public IWarningParser {
public:
    MOCK_METHOD(std::vector<Warning>, parse_warnings, (const std::string&), (override));
};

class BatchModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_terminal_ = std::make_unique<MockTerminalBatch>();
        mock_filesystem_ = std::make_unique<MockFileSystemBatch>();
        mock_parser_ = std::make_unique<MockWarningParserBatch>();
        
        // Capture raw pointers before moving to app
        terminal_ptr_ = mock_terminal_.get();
        filesystem_ptr_ = mock_filesystem_.get();
        parser_ptr_ = mock_parser_.get();
    }

    std::unique_ptr<MockTerminalBatch> mock_terminal_;
    std::unique_ptr<MockFileSystemBatch> mock_filesystem_;
    std::unique_ptr<MockWarningParserBatch> mock_parser_;
    
    // Raw pointers for setting expectations
    MockTerminalBatch* terminal_ptr_;
    MockFileSystemBatch* filesystem_ptr_;
    MockWarningParserBatch* parser_ptr_;
};

TEST_F(BatchModeTest, ProcessesAllWarningsWithDefaultStyle)
{
    // Setup test warnings
    std::vector<Warning> test_warnings = {
        Warning{
            .file_path = "test1.cpp",
            .line_number = 10,
            .column_number = 5,
            .warning_type = "readability-magic-numbers",
            .message = "42 is a magic number",
            .function_lines = std::nullopt
        },
        Warning{
            .file_path = "test2.cpp",
            .line_number = 20,
            .column_number = 1,
            .warning_type = "modernize-use-auto",
            .message = "use auto instead",
            .function_lines = std::nullopt
        }
    };

    // Mock expectations
    EXPECT_CALL(*parser_ptr_, parse_warnings(testing::_))
        .WillOnce(testing::Return(test_warnings));
    
    EXPECT_CALL(*terminal_ptr_, is_interactive())
        .WillOnce(testing::Return(false));

    // Config for batch mode
    Config config{
        .input_file = "test_input.txt",
        .interactive = false,
        .default_style = NolintStyle::NOLINT_SPECIFIC,
        .dry_run = true,  // Don't actually modify files in test
        .load_session_file = "",
        .save_session_file = ""
    };

    // Create app and run
    NolintApp app(std::move(mock_terminal_), std::move(mock_filesystem_), std::move(mock_parser_));
    
    // Capture output
    std::ostringstream output;
    std::streambuf* orig = std::cout.rdbuf();
    std::cout.rdbuf(output.rdbuf());
    
    int result = app.run(config);
    
    // Restore output
    std::cout.rdbuf(orig);
    
    // Verify results
    EXPECT_EQ(result, 0);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, testing::HasSubstr("Found 2 warnings"));
    EXPECT_THAT(output_str, testing::HasSubstr("Running in batch mode"));
    EXPECT_THAT(output_str, testing::HasSubstr("// NOLINT(warning-type)"));
    EXPECT_THAT(output_str, testing::HasSubstr("Processed 2 warnings, created 2 suppressions"));
    EXPECT_THAT(output_str, testing::HasSubstr("Dry run - no files modified. 2 decisions made"));
}

TEST_F(BatchModeTest, FallsBackToNolintSpecificWhenDefaultUnavailable)
{
    // Setup warning that doesn't support NOLINT_BLOCK
    std::vector<Warning> test_warnings = {
        Warning{
            .file_path = "test.cpp",
            .line_number = 15,
            .column_number = 8,
            .warning_type = "readability-magic-numbers",
            .message = "42 is a magic number",
            .function_lines = std::nullopt  // No function_lines, so NOLINT_BLOCK not available
        }
    };

    EXPECT_CALL(*parser_ptr_, parse_warnings(testing::_))
        .WillOnce(testing::Return(test_warnings));
    
    EXPECT_CALL(*terminal_ptr_, is_interactive())
        .WillOnce(testing::Return(false));

    // Try to use NOLINT_BLOCK as default, but it should fall back to NOLINT_SPECIFIC
    Config config{
        .input_file = "test_input.txt",
        .interactive = false,
        .default_style = NolintStyle::NOLINT_BLOCK,  // Not available for this warning
        .dry_run = true,
        .load_session_file = "",
        .save_session_file = ""
    };

    NolintApp app(std::move(mock_terminal_), std::move(mock_filesystem_), std::move(mock_parser_));
    
    std::ostringstream output;
    std::streambuf* orig = std::cout.rdbuf();
    std::cout.rdbuf(output.rdbuf());
    
    int result = app.run(config);
    
    std::cout.rdbuf(orig);
    
    EXPECT_EQ(result, 0);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, testing::HasSubstr("Processed 1 warnings, created 1 suppressions"));
}

TEST_F(BatchModeTest, HandlesEmptyWarningsList)
{
    // Setup empty warnings
    std::vector<Warning> empty_warnings;

    EXPECT_CALL(*parser_ptr_, parse_warnings(testing::_))
        .WillOnce(testing::Return(empty_warnings));
    
    EXPECT_CALL(*terminal_ptr_, is_interactive())
        .WillOnce(testing::Return(false));

    Config config{
        .input_file = "empty_input.txt",
        .interactive = false,
        .default_style = NolintStyle::NOLINT_SPECIFIC,
        .dry_run = true,
        .load_session_file = "",
        .save_session_file = ""
    };

    NolintApp app(std::move(mock_terminal_), std::move(mock_filesystem_), std::move(mock_parser_));
    
    std::ostringstream output;
    std::streambuf* orig = std::cout.rdbuf();
    std::cout.rdbuf(output.rdbuf());
    
    int result = app.run(config);
    
    std::cout.rdbuf(orig);
    
    EXPECT_EQ(result, 0);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, testing::HasSubstr("No warnings found"));
}

} // namespace nolint