set(HEADERS
    transactions.h)

install(FILES ${HEADERS} DESTINATION ${INSTALL_INCLUDE_PATH}/event_stream/extensions)

add_subdirectory(test)
