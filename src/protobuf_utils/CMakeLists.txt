set(SOURCES
    compressed_fstream.cc
    protobuf_utils.cc
    protobuf_stream.cc)

add_library(protobuf_utils ${SOURCES})
target_link_libraries(protobuf_utils PUBLIC protobuf::libprotobuf Boost::filesystem Boost::iostreams fmt)
set_property(TARGET protobuf_utils PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET protobuf_utils PROPERTY PUBLIC_HEADER protobuf_utils.h protobuf_stream.h)

install(TARGETS protobuf_utils
        ARCHIVE DESTINATION ${INSTALL_LIB_PATH}
        PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDE_PATH}/protobuf_utils)
install(FILES protobuf_utils.py protobuf_stream.py DESTINATION ${INSTALL_PYTHON_PATH})

add_subdirectory(test)
