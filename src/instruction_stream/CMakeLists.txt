set(HEADERS instruction_stream.h)
set(SOURCES instruction_stream.cc)

add_library(instruction_stream ${SOURCES})
target_link_libraries(instruction_stream PUBLIC protobuf_utils INTERFACE protobuf::libprotobuf)
set_property(TARGET instruction_stream PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET instruction_stream PROPERTY PUBLIC_HEADER "${HEADERS}")

add_protobuf(
    instruction_stream
    INSTALL IMPORT_OTHERS
    CPP_TARGET instruction_stream
    PROTOS instruction_stream.proto
)

add_library(disasm disasm.cc)
target_include_directories(disasm
    PRIVATE ${CMAKE_PROJECT_SOURCE_DIR}/include/instruction_stream
    PUBLIC ${CMAKE_PROJECT_SOURCE_DIR}/include)
target_link_libraries(disasm capstone-static fmt)
set_property(TARGET disasm PROPERTY POSITION_INDEPENDENT_CODE ON)

install(TARGETS instruction_stream
        ARCHIVE DESTINATION ${INSTALL_LIB_PATH}
        PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDE_PATH}/instruction_stream)
install(FILES instruction_stream.py DESTINATION ${INSTALL_PYTHON_PATH})

add_subdirectory(tools)
add_subdirectory(test)
add_subdirectory(testing)
