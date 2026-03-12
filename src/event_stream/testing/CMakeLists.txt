add_executable(generate_es_test generate_es_test.cc)
target_link_libraries(generate_es_test event_stream fmt)

set(SOURCES event_stream_dummy.cc event_stream_dummy.h build_event_stream.h)

add_library(event_stream_testing ${SOURCES})
target_link_libraries(event_stream_testing PUBLIC fmt testing)
add_dependencies(event_stream_testing generate_es_test)
target_compile_definitions(event_stream_testing PUBLIC GENERATE_ES_BIN="$<TARGET_FILE:generate_es_test>")
