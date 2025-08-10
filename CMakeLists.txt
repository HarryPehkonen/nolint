cmake_minimum_required(VERSION 3.20)
project(nolint VERSION 1.0.0 LANGUAGES CXX)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Find required packages
find_package(GTest REQUIRED)
include(GoogleTest)

# Smart FTXUI detection: try system package first, fallback to FetchContent
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
    message(STATUS "System FTXUI not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
        GIT_TAG v6.1.9
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(ftxui)
    message(STATUS "FTXUI integrated via FetchContent")
else()
    message(STATUS "Using system-installed FTXUI")
endif()

# Include directories
include_directories(include)

# Source files
set(NOLINT_SOURCES
    src/main.cpp
    src/ui_model.cpp
    src/file_context.cpp
    src/warning_parser.cpp
    src/annotated_file.cpp
    src/file_modifier.cpp
)

# Main executable with automatic piped input detection
add_executable(nolint ${NOLINT_SOURCES})
target_link_libraries(nolint PRIVATE 
    ftxui::component
    ftxui::dom
    ftxui::screen
)

# Tests
enable_testing()
add_subdirectory(tests)

# Quality assurance targets
add_custom_target(format
    COMMAND clang-format -i ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*/*.cpp ${CMAKE_SOURCE_DIR}/src/*/*/*.cpp
    COMMAND clang-format -i ${CMAKE_SOURCE_DIR}/include/nolint/*.hpp ${CMAKE_SOURCE_DIR}/include/nolint/*/*.hpp
    COMMAND clang-format -i ${CMAKE_SOURCE_DIR}/tests/*.cpp ${CMAKE_SOURCE_DIR}/tests/*/*.cpp
    COMMENT "Formatting all source files with clang-format"
)

add_custom_target(tidy
    COMMAND clang-tidy ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*/*.cpp -- -std=c++20 -I${CMAKE_SOURCE_DIR}/include
    COMMENT "Running clang-tidy analysis on source files"
)

add_custom_target(quality
    COMMAND ${CMAKE_COMMAND} --build . --target format
    COMMAND ${CMAKE_COMMAND} --build . --target tidy
    COMMAND ${CMAKE_COMMAND} --build . --target nolint_tests
    COMMAND ./nolint_tests
    COMMENT "Running complete quality check: format → lint → build → test"
)

# Installation
install(TARGETS nolint DESTINATION bin)