add_cpp_test(test_protobuf_utils SOURCES test_compressed_fstream.cc test_delimited.cc)
target_link_libraries(test_protobuf_utils protobuf_utils protobuf::libprotobuf testing)

add_protobuf(
    example
    CPP_TARGET test_protobuf_utils
    PROTOS example.proto
)

add_python_test(protobuf_python_test DISCOVER ${CMAKE_CURRENT_SOURCE_DIR} PATH ${CMAKE_CURRENT_BINARY_DIR})
