set(TESTS
    test_writer.cc
    test_reader.cc)

add_cpp_test(test_instruction_stream SOURCES ${TESTS})
target_link_libraries(test_instruction_stream instruction_stream testing)
target_compile_options(test_instruction_stream PRIVATE -Wno-array-bounds -Wno-sign-compare)

add_python_test(instruction_stream_python_test DISCOVER ${CMAKE_CURRENT_SOURCE_DIR})
