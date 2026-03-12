set(HEADERS branch_stream.h)
set(SOURCES branch_stream.cc)

add_library(branch_stream ${SOURCES})
target_include_directories(branch_stream PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../instruction_stream)
target_link_libraries(branch_stream PUBLIC instruction_stream protobuf_utils INTERFACE protobuf::libprotobuf)
target_compile_options(branch_stream PRIVATE -Wno-array-bounds)
set_property(TARGET branch_stream PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET branch_stream PROPERTY PUBLIC_HEADER "${HEADERS}")

add_protobuf(
    branch_stream
    INSTALL IMPORT_OTHERS
    CPP_TARGET branch_stream
    PROTOS branch_stream.proto
)

install(TARGETS branch_stream
        ARCHIVE DESTINATION ${INSTALL_LIB_PATH}
        PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDE_PATH}/branch_stream)

add_subdirectory(tools)
add_subdirectory(test)
