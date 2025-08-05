cmake_minimum_required(VERSION 3.20)
project(nolint VERSION 1.0.0 LANGUAGES CXX)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(PkgConfig QUIET)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Include directories
include_directories(include)

# Source files
set(NOLINT_SOURCES
    src/functional_core.cpp
    src/warning_parser.cpp
    src/nolint_app.cpp
)

# Main executable
add_executable(nolint 
    src/main.cpp 
    ${NOLINT_SOURCES}
)

# Test executable
add_executable(nolint_tests
    tests/test_main.cpp
    tests/functional_core_test.cpp
    tests/warning_parser_test.cpp
    tests/real_output_test.cpp
    tests/ui_integration_test.cpp
    ${NOLINT_SOURCES}
)

target_link_libraries(nolint_tests 
    PRIVATE 
    GTest::gtest 
    GTest::gmock 
    GTest::gtest_main
)

# Discover tests
gtest_discover_tests(nolint_tests)

# Custom targets for development workflow
add_custom_target(format
    COMMAND clang-format -i src/*.cpp include/*.hpp tests/*.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting code with clang-format"
)

add_custom_target(tidy
    COMMAND clang-tidy src/*.cpp -- -std=c++20 -I${CMAKE_SOURCE_DIR}/include
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy"
    DEPENDS nolint
)

# Make format run before build
add_dependencies(nolint format)
add_dependencies(nolint_tests format)