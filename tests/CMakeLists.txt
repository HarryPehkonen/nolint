cmake_minimum_required(VERSION 3.20)

# Find GTest
find_package(GTest REQUIRED)

# Test executable
add_executable(nolint_tests
    test_ui_model.cpp
    test_warning_parser.cpp
    test_file_context.cpp
    test_annotated_file.cpp
    # Add test sources from main project (but not main.cpp)
    ../src/ui_model.cpp
    ../src/warning_parser.cpp
    ../src/file_context.cpp
    ../src/annotated_file.cpp
)

# Include directories
target_include_directories(nolint_tests PRIVATE ../include)

# Link libraries
target_link_libraries(nolint_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
enable_testing()
add_test(NAME nolint_tests COMMAND nolint_tests)