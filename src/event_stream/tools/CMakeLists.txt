add_executable(es_viewer viewer.cc)
target_link_libraries(es_viewer event_stream Boost::program_options)

install(TARGETS es_viewer DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
