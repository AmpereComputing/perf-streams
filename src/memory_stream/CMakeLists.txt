set(HEADERS memory_stream.h)
set(SOURCES memory_stream.cc)

add_library(memory_stream memory_stream.cc)
target_link_libraries(memory_stream PUBLIC protobuf_utils INTERFACE protobuf::libprotobuf)
target_compile_options(memory_stream PRIVATE -Wno-array-bounds)
set_property(TARGET memory_stream PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET memory_stream PROPERTY PUBLIC_HEADER "${HEADERS}")

add_protobuf(
    memory_stream
    INSTALL IMPORT_OTHERS
    CPP_TARGET memory_stream
    PROTOS memory_stream.proto
)

install(TARGETS memory_stream
        ARCHIVE DESTINATION ${INSTALL_LIB_PATH}
        PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDE_PATH}/memory_stream)

add_subdirectory(tools)
add_subdirectory(test)
