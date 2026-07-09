set(HEADERS
    transactions.h)
set(PYTHON_SOURCES
    transactions.py)

install(FILES ${HEADERS} DESTINATION ${INSTALL_INCLUDE_PATH}/event_stream/extensions)
install(FILES ${PYTHON_SOURCES} DESTINATION ${INSTALL_PYTHON_PATH}/extensions)

add_subdirectory(test)
