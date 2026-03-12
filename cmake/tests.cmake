# Add a C++ (gtest) based test
#
# Arguments
# =========
# SOURCES: test sources
# LABELS: test labels
function(add_cpp_test target)
    cmake_parse_arguments(CPP_TEST "" "" "SOURCES;LABELS" ${ARGN})
    add_executable(${target} ${CPP_TEST_SOURCES})
    add_test(NAME ${target} COMMAND ${target})
    target_link_libraries(${target} gtest_main gmock)
    target_compile_definitions(${target} PRIVATE TEST_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${target} PRIVATE TEST_OUTPUT_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# Add a python (unittest) based test
#
# Arguments
# =========
# DISCOVER: run all tests within specified directory
# PATH: additional python path
# TESTS: python test files
# LABELS: test labels
function(add_python_test target)
    cmake_parse_arguments(PYTHON_TEST "" "DISCOVER;PATH" "TESTS;LABELS" ${ARGN})
    set(default_path ${CMAKE_INSTALL_PREFIX}/lib/python:$ENV{PYTHONPATH})
    if(PYTHON_TEST_PATH)
        string(PREPEND PYTHON_TEST_PATH ${default_path}:)
    else()
        set(PYTHON_TEST_PATH ${default_path})
    endif()
    if(PYTHON_TEST_TESTS)
        # Fix-up test paths since unittest only really supports modules
        list(GET PYTHON_TEST_TESTS 0 first_test)
        cmake_path(GET first_test PARENT_PATH test_module_directory)
        if(test_module_directory)
            cmake_path(IS_PREFIX CMAKE_CURRENT_SOURCE_DIR "${test_module_directory}" within_source)
            if(within_source)
                # Use source directory to allow relative module import
                set(test_module_directory ${CMAKE_CURRENT_SOURCE_DIR})
            endif()
        else()
            set(test_module_directory ${CMAKE_CURRENT_SOURCE_DIR})
        endif()
        string(PREPEND PYTHON_TEST_PATH ${test_module_directory}:)
        list(TRANSFORM PYTHON_TEST_TESTS REPLACE ${test_module_directory}/ "")
        list(TRANSFORM PYTHON_TEST_TESTS REPLACE "/" ".")
        list(TRANSFORM PYTHON_TEST_TESTS REPLACE ".py" "")
    endif()
    set(cmd /usr/bin/env PYTHONPATH=${PYTHON_TEST_PATH} TEST_COLLATERAL_DIR=${TEST_COLLATERAL_DIR} ${Python3_EXECUTABLE} -m)
    set(args "")
    if(CMAKE_BUILD_TYPE_NORMALIZED STREQUAL "COVERAGE")
        list(APPEND cmd coverage run -p --rcfile=${PROJECT_SOURCE_DIR}/.coveragerc -m)
    endif()
    if($ENV{UNITTEST_OUTPUT} MATCHES "xml:*")
        string(REPLACE "xml:" "./${target}/" xml_output_path $ENV{UNITTEST_OUTPUT})
        list(APPEND cmd xmlrunner)
        list(APPEND args -o ${xml_output_path})
    else()
        list(APPEND cmd unittest)
    endif()
    if(PYTHON_TEST_DISCOVER)
        list(APPEND cmd discover ${PYTHON_TEST_DISCOVER})
    endif()
    list(APPEND cmd -v -c -b --locals)
    list(APPEND PYTHON_TEST_LABELS python)
    list(APPEND args ${PYTHON_TEST_TESTS})
    add_test(NAME ${target} COMMAND ${cmd} ${args} COMMAND_EXPAND_LISTS)
    set_property(TEST ${target} PROPERTY LABELS ${PYTHON_TEST_LABELS})
    if(CMAKE_BUILD_TYPE_NORMALIZED STREQUAL "COVERAGE")
        set_property(TEST ${target} PROPERTY ENVIRONMENT COVERAGE_PROCESS_START=1)
    endif()
endfunction()
