cmake_minimum_required(VERSION 3.14)
project(nolint VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source files
set(SOURCE_FILES
    src/main.cpp
    src/warning_parser.cpp
    src/file_manager.cpp
    src/simple_ui.cpp
    src/context_builder.cpp
    src/nolint_processor.cpp
)

set(HEADER_FILES
    include/nolint/warning_parser.hpp
    include/nolint/file_manager.hpp
    include/nolint/simple_ui.hpp
    include/nolint/context_builder.hpp
    include/nolint/nolint_processor.hpp
    include/nolint/interfaces.hpp
    include/nolint/types.hpp
)

# Main executable
add_executable(nolint ${SOURCE_FILES})
target_include_directories(nolint PRIVATE include)

# Testing
enable_testing()
find_package(GTest REQUIRED)

# Test source files (excluding main.cpp)
set(TEST_SOURCE_FILES
    src/warning_parser.cpp
    src/file_manager.cpp
    src/simple_ui.cpp
    src/context_builder.cpp
    src/nolint_processor.cpp
)

set(TEST_FILES
    tests/test_file_manager.cpp
)

add_executable(nolint_tests ${TEST_FILES} ${TEST_SOURCE_FILES})
target_include_directories(nolint_tests PRIVATE include)
target_link_libraries(nolint_tests GTest::gtest GTest::gmock GTest::gtest_main)
target_compile_definitions(nolint_tests PRIVATE UNIT_TEST)

# All source and header files for formatting/linting
set(ALL_SOURCE_FILES ${SOURCE_FILES} ${HEADER_FILES} ${TEST_FILES})

# Format target
add_custom_target(format
    COMMAND clang-format -i ${ALL_SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on all source files"
)

# Tidy target
add_custom_target(tidy
    COMMAND clang-tidy ${SOURCE_FILES} -- -I${CMAKE_SOURCE_DIR}/include -std=c++20
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy on source files"
)

# Check format target (doesn't modify files)
add_custom_target(check-format
    COMMAND clang-format --dry-run --Werror ${ALL_SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking code formatting"
)

# Custom targets that run before build
if(RUN_CLANG_FORMAT)
    add_custom_target(pre-build-format
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target format
    )
    add_dependencies(nolint pre-build-format)
endif()

# Installation
install(TARGETS nolint
    RUNTIME DESTINATION bin
)

# Add test
add_test(NAME nolint_tests COMMAND nolint_tests)