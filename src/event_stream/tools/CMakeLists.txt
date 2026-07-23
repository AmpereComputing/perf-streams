add_executable(es_viewer viewer.cc)
target_link_libraries(es_viewer event_stream Boost::program_options)

add_executable(o3pipeview_to_es o3pipeview_to_es.cc)
target_link_libraries(o3pipeview_to_es event_stream Boost::program_options)

install(TARGETS es_viewer o3pipeview_to_es DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
