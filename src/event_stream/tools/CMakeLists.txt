add_executable(es_viewer viewer.cc)
target_link_libraries(es_viewer event_stream Boost::program_options)

add_executable(gem5_o3pipeview_to_es gem5_o3pipeview_to_es.cc)
target_link_libraries(gem5_o3pipeview_to_es event_stream Boost::program_options)

install(TARGETS es_viewer gem5_o3pipeview_to_es DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
