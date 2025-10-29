cmake_minimum_required(VERSION 3.22)
project(tom_mole LANGUAGES CXX C)

# -----------------------------
# Settings
# -----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# -----------------------------
# Find Packages
# -----------------------------
find_package(Zstd REQUIRED)

# Currently, this project only targets macOS using Homebrew
# [TODO] Add more support
if(APPLE)
    execute_process(
            COMMAND brew --prefix llvm
            OUTPUT_VARIABLE BREW_LLVM_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LLVM_DIR "${BREW_LLVM_PREFIX}/lib/cmake/llvm")
endif()

if(NOT LLVM_DIR)
    message(FATAL_ERROR "LLVM_DIR is not set. Please install LLVM or set LLVM_DIR.")
endif()

list(APPEND CMAKE_PREFIX_PATH "${LLVM_DIR}/lib/cmake/llvm")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}/lib/cmake/llvm")


# -----------------------------
# Libs
# -----------------------------
file(GLOB TOM_MOLE_SRC "src/compiler/*.cpp")
add_library(tom_mole_compiler ${TOM_MOLE_SRC})

target_include_directories(tom_mole_compiler
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LLVM_INCLUDE_DIRS}
)
target_compile_definitions(tom_mole_compiler PRIVATE ${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs
        core
        orcjit
        native
        support
)

target_link_libraries(tom_mole_compiler PRIVATE ${llvm_libs})


# -----------------------------
# Bins
# -----------------------------
add_executable(tom_mole_exec src/app/main.cpp)
target_link_libraries(tom_mole_exec PRIVATE tom_mole_compiler)
