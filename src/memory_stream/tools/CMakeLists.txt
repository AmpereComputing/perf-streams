add_executable(ms_viewer viewer.cc)
target_link_libraries(ms_viewer memory_stream fmt)

install(TARGETS ms_viewer DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
