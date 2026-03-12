set(SOURCES
    args.cc
    args.h
    counter.cc
    counter.h
    factor.cc
    factor.h
    main.cc
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

add_executable(evp ${SOURCES})
target_link_libraries(evp event_stream util fmt Python3::Python stdc++fs)

install(TARGETS evp DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(plugins)
add_subdirectory(test)
