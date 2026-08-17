cmake_minimum_required(VERSION 3.20)
project(
        astra
        VERSION 0.1.0
        LANGUAGES CXX C
)

# Global C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# RTTI is off project-wide (LLVM-style). This keeps new modules aligned with
# the -fno-rtti-built LLVM libraries.
# The ANTLR-generated parser/lexer code requires RTTI and re-enables it
# in grammars/CMakeLists.txt.
add_compile_options(-fno-rtti)
# The strict warning flags live in src/, tests/ and tools/ (project code only):
# `add_compile_options` is a directory property that propagates to targets
# created in this directory and below, so setting it here would also apply to
# third-party sources built by FetchContent (Catch2). Project subdirectories
# opt into the warnings explicitly.

# Options
option(BUILD_TOOLS "Build tools" ON)
option(BUILD_TESTS "Build tests" ON)

# CTest
include(CTest)

# Include paths
include_directories(${PROJECT_SOURCE_DIR}/include)

# Third-party dependencies
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Antlr4
set_property(DIRECTORY PROPERTY EP_UPDATE_DISCONNECTED TRUE)
include(ExternalAntlr4Cpp)
# SYSTEM: treat third-party headers as system headers (-isystem), so the
# strict warning flags above do not fire inside them.
include_directories(SYSTEM ${ANTLR4_INCLUDE_DIRS})
find_package(ANTLR REQUIRED)

# LLVM
find_package(LLVM REQUIRED CONFIG)
# SYSTEM: as above — LLVM headers must not be subject to our warnings.
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

Include(FetchContent)

# Catch2
if (BUILD_TESTS)
  FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.8.1 # or a later release
    )
  FetchContent_MakeAvailable(Catch2)
endif ()

add_subdirectory(grammars)
add_subdirectory(src)
if (BUILD_TOOLS)
  add_subdirectory(tools)
endif ()

if (BUILD_TESTS)
  add_subdirectory(tests)
endif ()

# format
file(GLOB_RECURSE ALL_SOURCE_FILE
        src/*.cpp include/*.h include/*.def tests/*.h tests/*.cpp tools/*.h
        tools/*.cpp)

add_custom_target(
        format
        COMMAND clang-format-18 -i ${ALL_SOURCE_FILE}
        COMMENT "Running clang-format on source files"
)
