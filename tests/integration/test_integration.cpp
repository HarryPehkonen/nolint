#include "nolint/application/nolint_app.hpp"
#include "nolint/io/file_system.hpp" 
#include "nolint/parsers/warning_parser.hpp"
#include "nolint/ui/terminal.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace nolint {

// Mock implementations for testing
class MockTerminal : public ITerminal {
public:
    MOCK_METHOD(bool, setup_raw_mode, (), (override));
    MOCK_METHOD(InputEvent, get_input_event, (), (override));
    MOCK_METHOD(void, display_screen, (const Screen& screen), (override));
    MOCK_METHOD(std::string, read_line, (), (override));
    MOCK_METHOD(bool, is_interactive, (), (override));
    MOCK_METHOD(void, restore_terminal_state, (), (override));
};

class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(AnnotatedFile, read_file_to_annotated, (const std::string& path), (override));
    MOCK_METHOD(bool, write_annotated_file, (const AnnotatedFile& file, const std::string& path), (override));
    MOCK_METHOD(bool, file_exists, (const std::string& path), (override));
};

class MockWarningParser : public IWarningParser {
public:
    MOCK_METHOD(std::vector<Warning>, parse_warnings, (const std::string& clang_tidy_output), (override));
};

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        mock_terminal_ = std::make_unique<MockTerminal>();
        mock_filesystem_ = std::make_unique<MockFileSystem>();
        mock_parser_ = std::make_unique<MockWarningParser>();
        
        // Store raw pointers for expectations
        terminal_ptr_ = mock_terminal_.get();
        filesystem_ptr_ = mock_filesystem_.get();
        parser_ptr_ = mock_parser_.get();
    }
    
    std::unique_ptr<MockTerminal> mock_terminal_;
    std::unique_ptr<MockFileSystem> mock_filesystem_;
    std::unique_ptr<MockWarningParser> mock_parser_;
    
    MockTerminal* terminal_ptr_;
    MockFileSystem* filesystem_ptr_;
    MockWarningParser* parser_ptr_;
    
    std::vector<Warning> sample_warnings_{
        Warning{
            .file_path = "/test.cpp",
            .line_number = 1,
            .column_number = 1,
            .warning_type = "readability-magic-numbers",
            .message = "test warning",
            .function_lines = std::nullopt
        }
    };
};

TEST_F(IntegrationTest, NonInteractiveMode)
{
    // Setup expectations
    EXPECT_CALL(*terminal_ptr_, is_interactive())
        .WillOnce(::testing::Return(false));
    
    EXPECT_CALL(*parser_ptr_, parse_warnings(testing::_))
        .WillOnce(::testing::Return(sample_warnings_));
    
    // Create app
    NolintApp app(std::move(mock_terminal_), std::move(mock_filesystem_), std::move(mock_parser_));
    
    // Run with non-interactive config
    Config config;
    config.interactive = false;
    
    // Note: This will return 1 because non-interactive mode is not implemented yet
    int result = app.run(config);
    EXPECT_EQ(result, 1);  // Expected to fail until implemented
}

TEST_F(IntegrationTest, DryRunMode)
{
    // Setup expectations
    EXPECT_CALL(*terminal_ptr_, is_interactive())
        .WillOnce(::testing::Return(false));
    
    EXPECT_CALL(*parser_ptr_, parse_warnings(testing::_))
        .WillOnce(::testing::Return(sample_warnings_));
    
    // Create app  
    NolintApp app(std::move(mock_terminal_), std::move(mock_filesystem_), std::move(mock_parser_));
    
    // Run with dry-run config
    Config config;
    config.dry_run = true;
    config.interactive = false;
    
    int result = app.run(config);
    EXPECT_EQ(result, 1);  // Expected until non-interactive is implemented
}

TEST_F(IntegrationTest, EmptyWarningList)
{
    // Setup expectations
    EXPECT_CALL(*parser_ptr_, parse_warnings(testing::_))
        .WillOnce(::testing::Return(std::vector<Warning>{}));
    
    // Create app
    NolintApp app(std::move(mock_terminal_), std::move(mock_filesystem_), std::move(mock_parser_));
    
    Config config;
    int result = app.run(config);
    EXPECT_EQ(result, 0);  // Should succeed with no warnings
}

} // namespace nolint