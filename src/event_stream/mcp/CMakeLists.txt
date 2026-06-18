set(MCP_SOURCES
    query.cc
    query.h)

add_library(event_stream_mcp_core ${MCP_SOURCES})
target_link_libraries(event_stream_mcp_core PUBLIC event_stream event_stream_processor_core fmt)
set_property(TARGET event_stream_mcp_core PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(_event_stream_mcp MODULE python_module.cc)
target_link_libraries(_event_stream_mcp PRIVATE event_stream_mcp_core Python3::Python fmt)
set_target_properties(_event_stream_mcp PROPERTIES PREFIX "" SUFFIX ".so")

install(TARGETS _event_stream_mcp LIBRARY DESTINATION ${INSTALL_PYTHON_PATH})
install(FILES event_stream_mcp.py DESTINATION ${INSTALL_PYTHON_PATH})
install(PROGRAMS es_mcp DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
