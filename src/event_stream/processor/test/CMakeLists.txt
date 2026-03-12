set(TESTS
    test_evp.cc
    test_frame.cc)

add_cpp_test(test_evp SOURCES ${TESTS})
target_link_libraries(test_evp fmt event_stream_testing testing)
target_compile_definitions(test_evp PRIVATE BINDIR="${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(test_evp PRIVATE -Wno-sign-compare)
set_tests_properties(test_evp PROPERTIES ENVIRONMENT "EVP_CONFIG_PATH=${CMAKE_CURRENT_SOURCE_DIR}/configs")

add_dependencies(test_evp evp)
