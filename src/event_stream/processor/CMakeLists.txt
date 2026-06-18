set(SOURCES
    args.cc
    args.h
    counter.cc
    counter.h
    factor.cc
    factor.h
    metric_table.cc
    metric_table.h
    plugin.cc
    plugin.h
    processor_ifc.cc
    processor_ifc.h
    processor.cc
    processor.h
    strnatcmp.c
    strnatcmp.h
    utils.cc
    utils.h)

add_library(event_stream_processor_core ${SOURCES})
target_link_libraries(event_stream_processor_core PUBLIC event_stream fmt Python3::Python stdc++fs)
target_link_libraries(event_stream_processor_core PRIVATE util)
set_property(TARGET event_stream_processor_core PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(evp main.cc)
target_link_libraries(evp event_stream_processor_core)

install(TARGETS evp DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(plugins)
add_subdirectory(test)
