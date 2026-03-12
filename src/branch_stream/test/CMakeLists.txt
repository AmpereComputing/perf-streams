set(TESTS
    test_writer.cc
    test_reader.cc)

add_cpp_test(test_branch_stream SOURCES ${TESTS})
target_link_libraries(test_branch_stream branch_stream testing)
target_compile_options(test_branch_stream PRIVATE -Wno-sign-compare -Wno-array-bounds)
