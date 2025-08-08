#include "nolint/ui/ui_model.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace nolint {

class UIModelPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = std::filesystem::temp_directory_path() / "nolint_test_decisions.txt";
    }

    void TearDown() override {
        // Clean up test file
        std::error_code ec;
        std::filesystem::remove(test_file_, ec);
    }

    std::filesystem::path test_file_;
};

TEST_F(UIModelPersistenceTest, SaveEmptyDecisions)
{
    Decisions empty_decisions;
    EXPECT_TRUE(save_decisions(empty_decisions, test_file_.string()));
    
    // File should exist but be empty (since we only save non-NONE decisions)
    std::ifstream file(test_file_);
    EXPECT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.empty());
}

TEST_F(UIModelPersistenceTest, SaveAndLoadDecisions)
{
    Decisions original_decisions = {
        {"file1.cpp:10:5", NolintStyle::NOLINT_SPECIFIC},
        {"file2.cpp:20:1", NolintStyle::NOLINTNEXTLINE},
        {"file3.cpp:30:8", NolintStyle::NOLINT_BLOCK},
        {"file4.cpp:40:2", NolintStyle::NONE}  // This should not be saved
    };

    // Save decisions
    EXPECT_TRUE(save_decisions(original_decisions, test_file_.string()));

    // Load decisions
    auto loaded_decisions = load_decisions(test_file_.string());
    ASSERT_TRUE(loaded_decisions.has_value());

    // Check that non-NONE decisions were saved and loaded correctly
    EXPECT_EQ(loaded_decisions->at("file1.cpp:10:5"), NolintStyle::NOLINT_SPECIFIC);
    EXPECT_EQ(loaded_decisions->at("file2.cpp:20:1"), NolintStyle::NOLINTNEXTLINE);
    EXPECT_EQ(loaded_decisions->at("file3.cpp:30:8"), NolintStyle::NOLINT_BLOCK);
    
    // NONE decision should not be saved/loaded
    EXPECT_EQ(loaded_decisions->find("file4.cpp:40:2"), loaded_decisions->end());
    
    // Should have 3 decisions (excluding NONE)
    EXPECT_EQ(loaded_decisions->size(), 3);
}

TEST_F(UIModelPersistenceTest, LoadNonexistentFile)
{
    auto decisions = load_decisions("/nonexistent/path/decisions.txt");
    EXPECT_FALSE(decisions.has_value());
}

TEST_F(UIModelPersistenceTest, SaveToInvalidPath)
{
    Decisions decisions = {
        {"file1.cpp:10:5", NolintStyle::NOLINT_SPECIFIC}
    };
    
    // Try to save to invalid path
    EXPECT_FALSE(save_decisions(decisions, "/invalid/readonly/path/decisions.txt"));
}

TEST_F(UIModelPersistenceTest, LoadMalformedFile)
{
    // Create malformed file
    {
        std::ofstream file(test_file_);
        file << "malformed line without pipe\n";
        file << "file1.cpp:10:5|NOLINT_SPECIFIC\n";  // Valid line
        file << "another|malformed|too|many|pipes\n";
        file << "\n";  // Empty line
        file << "file2.cpp:20:1|NOLINTNEXTLINE\n";  // Another valid line
    }

    auto decisions = load_decisions(test_file_.string());
    ASSERT_TRUE(decisions.has_value());

    // Should only load valid lines
    EXPECT_EQ(decisions->size(), 2);
    EXPECT_EQ(decisions->at("file1.cpp:10:5"), NolintStyle::NOLINT_SPECIFIC);
    EXPECT_EQ(decisions->at("file2.cpp:20:1"), NolintStyle::NOLINTNEXTLINE);
}

TEST_F(UIModelPersistenceTest, HandleUnknownStyles)
{
    // Create file with unknown style
    {
        std::ofstream file(test_file_);
        file << "file1.cpp:10:5|UNKNOWN_STYLE\n";
        file << "file2.cpp:20:1|NOLINT_SPECIFIC\n";
    }

    auto decisions = load_decisions(test_file_.string());
    ASSERT_TRUE(decisions.has_value());

    // Unknown style should default to NONE
    EXPECT_EQ(decisions->at("file1.cpp:10:5"), NolintStyle::NONE);
    EXPECT_EQ(decisions->at("file2.cpp:20:1"), NolintStyle::NOLINT_SPECIFIC);
}

TEST_F(UIModelPersistenceTest, RoundTripPersistence)
{
    Decisions original = {
        {"src/parser.cpp:42:10", NolintStyle::NOLINT_SPECIFIC},
        {"include/header.hpp:15:1", NolintStyle::NOLINTNEXTLINE},
        {"tests/test.cpp:100:5", NolintStyle::NOLINT_BLOCK}
    };

    // Save then load
    EXPECT_TRUE(save_decisions(original, test_file_.string()));
    auto loaded = load_decisions(test_file_.string());
    
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, original);
}

} // namespace nolint