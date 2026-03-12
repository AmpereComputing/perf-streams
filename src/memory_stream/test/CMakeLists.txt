set(TESTS
    test_writer.cc
    test_reader.cc)

add_cpp_test(test_memory_stream SOURCES ${TESTS})
target_link_libraries(test_memory_stream memory_stream testing)
target_compile_options(test_memory_stream PRIVATE -Wno-sign-compare -Wno-array-bounds)
