#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "nolint/file_manager.hpp"
#include "nolint/types.hpp"
#include <memory>

using namespace nolint;
using ::testing::Return;

// Mock file system for testing
class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(std::vector<std::string>, read_file, (const std::string& path), (override));
    MOCK_METHOD(void, write_file, (const std::string& path, const std::vector<std::string>& lines), (override));
    MOCK_METHOD(bool, file_exists, (const std::string& path), (override));
};

class FileManagerTest : public ::testing::Test {
protected:
    std::unique_ptr<MockFileSystem> mock_fs;
    std::unique_ptr<FileManager> file_manager;

    void SetUp() override {
        mock_fs = std::make_unique<MockFileSystem>();
    }

    void CreateFileManager() {
        file_manager = std::make_unique<FileManager>(std::move(mock_fs));
    }
};

TEST_F(FileManagerTest, ExtractIndentationWithSpaces) {
    // Arrange
    std::vector<std::string> file_contents = {
        "#include <iostream>",
        "",
        "int main() {",
        "    int x = 42;",  // 4 spaces
        "    return 0;",
        "}"
    };
    
    EXPECT_CALL(*mock_fs, read_file("/test.cpp"))
        .WillOnce(Return(file_contents));
    
    CreateFileManager();
    
    // Act
    std::string indent = file_manager->extract_indentation("/test.cpp", 4);
    
    // Assert
    EXPECT_EQ(indent, "    ");  // Should be 4 spaces
    EXPECT_EQ(indent.length(), 4u);
}

TEST_F(FileManagerTest, ExtractIndentationWithTabs) {
    // Arrange
    std::vector<std::string> file_contents = {
        "void foo() {",
        "\tint x = 42;",  // 1 tab
        "}"
    };
    
    EXPECT_CALL(*mock_fs, read_file("/test.cpp"))
        .WillOnce(Return(file_contents));
    
    CreateFileManager();
    
    // Act
    std::string indent = file_manager->extract_indentation("/test.cpp", 2);
    
    // Assert
    EXPECT_EQ(indent, "\t");
    EXPECT_EQ(indent.length(), 1u);
}

TEST_F(FileManagerTest, NolintnextlineIndentation) {
    // Arrange
    std::vector<std::string> file_contents = {
        "#include <iostream>",
        "",
        "int main() {",
        "    int x = 42;",
        "    return 0;",
        "}"
    };
    
    EXPECT_CALL(*mock_fs, read_file("/test.cpp"))
        .WillOnce(Return(file_contents))
        .WillOnce(Return(file_contents));  // For apply_modification
    
    EXPECT_CALL(*mock_fs, write_file("/test.cpp", ::testing::_))
        .WillOnce([](const std::string&, const std::vector<std::string>& lines) {
            // Verify the NOLINTNEXTLINE is properly indented
            EXPECT_EQ(lines[3], "    // NOLINTNEXTLINE(readability-magic-numbers)");
            EXPECT_EQ(lines[4], "    int x = 42;");
        });
    
    CreateFileManager();
    
    // Act
    Modification mod;
    mod.line_number = 4;  // Line with "int x = 42;"
    mod.style = NolintStyle::NOLINTNEXTLINE;
    mod.warning_type = "readability-magic-numbers";
    mod.indent = file_manager->extract_indentation("/test.cpp", 4);
    
    file_manager->apply_modification("/test.cpp", mod);
    file_manager->write_file("/test.cpp");
}

TEST_F(FileManagerTest, MixedIndentationLevels) {
    // Arrange
    std::vector<std::string> file_contents = {
        "void process() {",
        "    if (condition) {",
        "        for (int i = 0; i < 10; ++i) {",
        "            doSomething();",  // 12 spaces
        "        }",
        "    }",
        "}"
    };
    
    EXPECT_CALL(*mock_fs, read_file("/test.cpp"))
        .WillOnce(Return(file_contents));
    
    CreateFileManager();
    
    // Act
    std::string indent = file_manager->extract_indentation("/test.cpp", 4);
    
    // Assert
    EXPECT_EQ(indent, "            ");  // Should be 12 spaces
    EXPECT_EQ(indent.length(), 12u);
}

// Test the UI display logic would go in a separate test file for SimpleUI
// but that would require mocking terminal I/O which is more complex