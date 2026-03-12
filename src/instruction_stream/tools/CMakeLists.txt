add_executable(is_viewer viewer.cc)
target_link_libraries(is_viewer instruction_stream disasm fmt)

add_executable(is_info info.cc)
target_link_libraries(is_info instruction_stream)

add_executable(is_chop chop.cc)
target_link_libraries(is_chop instruction_stream)

add_executable(is_disasm disasm.cc)
target_link_libraries(is_disasm instruction_stream disasm fmt)

install(TARGETS is_viewer is_info is_disasm is_chop DESTINATION ${INSTALL_BIN_PATH})

add_subdirectory(test)
