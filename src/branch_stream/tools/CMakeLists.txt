add_executable(bs_viewer viewer.cc)
target_link_libraries(bs_viewer branch_stream fmt)

add_executable(bs_chop chop.cc)
target_link_libraries(bs_chop branch_stream)

install(TARGETS bs_viewer bs_chop DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
