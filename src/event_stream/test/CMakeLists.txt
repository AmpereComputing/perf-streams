set(TESTS
    test_event.cc
    test_writer.cc
    test_reader.cc)

add_cpp_test(test_event_stream SOURCES ${TESTS})
target_link_libraries(test_event_stream event_stream event_stream_testing)
target_compile_options(test_event_stream PRIVATE -Wno-array-bounds)

add_python_test(event_stream_python_test DISCOVER ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(event_stream_python_test PROPERTIES DEPENDS test_event_stream)
