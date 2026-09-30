cmake_minimum_required(VERSION 3.24)

project(perf_streams CXX C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "default install path" FORCE )
endif()

if(CMAKE_BUILD_TYPE)
    string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_NORMALIZED)
else()
    set(CMAKE_BUILD_TYPE_NORMALIZED RELEASE)
endif()

set(INSTALL_BIN_PATH bin)
set(INSTALL_INCLUDE_PATH include)
set(INSTALL_LIB_PATH lib)
set(INSTALL_PYTHON_PATH lib/python/perf_streams)

if(DEFINED SKBUILD_SCRIPTS_DIR)
    set(INSTALL_BIN_PATH ${SKBUILD_SCRIPTS_DIR})
endif()
if(DEFINED SKBUILD_HEADERS_DIR)
    set(INSTALL_INCLUDE_PATH ${SKBUILD_HEADERS_DIR})
endif()
if(DEFINED SKBUILD_PROJECT_NAME)
    set(INSTALL_PYTHON_PATH ${SKBUILD_PROJECT_NAME})
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
if(NOT ${CMAKE_BINARY_DIR} EQUAL ${PROJECT_SOURCE_DIR})
    file(RELATIVE_PATH relpath ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/compile_commands.json)
    execute_process(COMMAND ln -sf ${relpath} ${PROJECT_SOURCE_DIR})
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

option(CLANG_TIDY "Enable clang-tidy linting (requires compilation using clang)" ON)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY AND CLANG_TIDY_EXE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CLANG_TIDY_ARGS "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy-required;-p;${CMAKE_BINARY_DIR};--header-filter=^${CMAKE_SOURCE_DIR}/src/.*")
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_ARGS}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_ARGS}")
endif()

set(CMAKE_C_FLAGS_COVERAGE "--coverage")
set(CMAKE_CXX_FLAGS_COVERAGE "--coverage")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage")

set(CMAKE_C_FLAGS_ASAN "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -Wno-maybe-uninitialized")
set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -Wno-maybe-uninitialized")
set(CMAKE_EXE_LINKER_FLAGS_ASAN ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} "-fsanitize=address -fsanitize=undefined")

set(CMAKE_C_FLAGS_PROFILING "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg")
set(CMAKE_CXX_FLAGS_PROFILING "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
set(CMAKE_EXE_LINKER_FLAGS_PROFILING "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -pg")

include(CTest)

find_package(PkgConfig REQUIRED)
find_package(capstone CONFIG QUIET)
if(NOT capstone_FOUND)
    pkg_check_modules(CAPSTONE REQUIRED IMPORTED_TARGET capstone)
    if(NOT CAPSTONE_FOUND)
        message(FATAL_ERROR "Unable to find capstone")
    endif()
    find_path(CAPSTONE_PUBLIC_INCLUDE_DIR capstone/capstone.h
        HINTS ${CAPSTONE_INCLUDE_DIRS}
        PATH_SUFFIXES ..)
    if(NOT CAPSTONE_PUBLIC_INCLUDE_DIR)
        message(FATAL_ERROR "Unable to find Capstone public headers")
    endif()
    add_library(capstone INTERFACE)
    target_link_libraries(capstone INTERFACE PkgConfig::CAPSTONE)
    target_include_directories(capstone INTERFACE ${CAPSTONE_PUBLIC_INCLUDE_DIR})
    add_library(capstone-static ALIAS capstone)
else()
    add_library(capstone ALIAS capstone::capstone)
    add_library(capstone-static ALIAS capstone::capstone)
endif()

find_package(Protobuf REQUIRED CONFIG)

find_package(Boost 1.65.0 REQUIRED CONFIG COMPONENTS filesystem iostreams process program_options regex)

find_package(GTest REQUIRED)
add_library(gtest ALIAS GTest::gtest)
add_library(gtest_main ALIAS GTest::gtest_main)
add_library(gmock ALIAS GTest::gmock)

find_package(fmt REQUIRED)
add_library(fmt ALIAS fmt::fmt-header-only)

find_package(Python3 COMPONENTS Interpreter Development)

include(cmake/tests.cmake)
include(cmake/protobuf.cmake)

add_compile_options(-Wall -Werror)
include_directories(src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src)
