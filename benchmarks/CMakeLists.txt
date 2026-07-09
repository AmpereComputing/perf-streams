add_executable(perf_streams_benchmarks event_stream_benchmarks.cc)
set_target_properties(perf_streams_benchmarks PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_definitions(perf_streams_benchmarks
                           PRIVATE EVP_PROGRAM="$<TARGET_FILE:evp>"
                                   BENCHMARK_DATA_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/data")
target_link_libraries(perf_streams_benchmarks PRIVATE event_stream benchmark::benchmark fmt)
add_dependencies(perf_streams_benchmarks evp)
