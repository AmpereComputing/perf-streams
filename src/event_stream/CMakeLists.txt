set(HEADERS
    event.h
    transaction.h
    event_definition.h
    event_stream.h
    event_stream_with.h
    event_stream_sink.h
    event_stream_proto.h
    event_stream_reader.h
    event_stream_recorder.h)
set(EXTENSION_HEADERS
    extensions/transactions.h)
set(SOURCES
    transaction.cc
    event_definition.cc
    event_stream.cc
    event_stream_proto.cc
    event_stream_sink.cc
    event_stream_reader.cc 
    event_stream_recorder.cc)

add_library(event_stream ${SOURCES})
target_link_libraries(event_stream PUBLIC protobuf_utils protobuf::libprotobuf Boost::boost fmt)
target_compile_options(event_stream PRIVATE -Wno-array-bounds)
set_property(TARGET event_stream PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET event_stream PROPERTY PUBLIC_HEADER "${HEADERS}")

add_protobuf(
    event_stream
    INSTALL IMPORT_OTHERS
    CPP_TARGET event_stream
    PROTOS event_stream.proto
)

install(TARGETS event_stream
        ARCHIVE DESTINATION ${INSTALL_LIB_PATH}
        PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDE_PATH}/event_stream)
install(FILES ${EXTENSION_HEADERS} DESTINATION ${INSTALL_INCLUDE_PATH}/event_stream/extensions)
install(FILES event_stream.py DESTINATION ${INSTALL_PYTHON_PATH})

add_subdirectory(processor)
add_subdirectory(tools)
add_subdirectory(test)
add_subdirectory(testing)
