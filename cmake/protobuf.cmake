# Generate C++ protobuf sources
#
# Arguments
# =========
# INSTALL: install C++ headers
# IMPORT_OTHERS: import path relative to src/
# OUT_VAR: output variable for sources
# PROTOS: input protobuf files
function(add_cpp_protobuf name target)
    cmake_parse_arguments(CPP_PROTOBUF "INSTALL;IMPORT_OTHERS" "OUT_VAR" "PROTOS" ${ARGN})
    set(extra_args)
    if(CPP_PROTOBUF_IMPORT_OTHERS)
        set(extra_args PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/.." IMPORT_DIRS "${PROJECT_SOURCE_DIR}/src")
    endif()
    protobuf_generate(
        TARGET ${target}
        OUT_VAR PROTO_CPP_SRCS
        LANGUAGE cpp
        PROTOS ${CPP_PROTOBUF_PROTOS}
        ${extra_args}
    )
    if(CPP_PROTOBUF_INSTALL)
        install(FILES ${PROTO_CPP_SRCS} DESTINATION ${INSTALL_INCLUDE_PATH}/${name})
    endif()
    if(CPP_PROTOBUF_OUT_VAR)
        set(${CPP_PROTOBUF_OUT_VAR} ${PROTO_CPP_SRCS} PARENT_SCOPE)
    endif()
endfunction()

# Generate python protobuf sources
#
# Arguments
# =========
# INSTALL: install python sources in python library/package path
# IMPORT_OTHERS: import path relative to src/
# TARGET: target used for (ordering) dependency
# OUT_VAR: output variable for sources
# PROTOS: input protobuf files
function(add_python_protobuf name)
    cmake_parse_arguments(PY_PROTOBUF "INSTALL;IMPORT_OTHERS" "TARGET;OUT_VAR" "PROTOS" ${ARGN})
    set(extra_args)
    if(PY_PROTOBUF_IMPORT_OTHERS)
        set(extra_args PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/.." IMPORT_DIRS "${PROJECT_SOURCE_DIR}/src")
    endif()
    protobuf_generate(
        OUT_VAR PROTO_PY_SRCS
        LANGUAGE python
        PROTOS ${PY_PROTOBUF_PROTOS}
        ${extra_args}
    )
    if(NOT PY_PROTOBUF_TARGET)
        set(PY_PROTOBUF_TARGET ${name}_proto_python)
    endif()
    add_custom_target(${PY_PROTOBUF_TARGET} ALL DEPENDS ${PROTO_PY_SRCS})
    if(PY_PROTOBUF_INSTALL)
        install(FILES ${PROTO_PY_SRCS} DESTINATION ${INSTALL_PYTHON_PATH})
    endif()
    if(PY_PROTOBUF_OUT_VAR)
        set(${PY_PROTOBUF_OUT_VAR} ${PROTO_PY_SRCS} PARENT_SCOPE)
    endif()
endfunction()

# Generate C++ and python protobuf sources
#
# Arguments
# =========
# CPP_TARGET: C++ target name
#
# * others forwarded to add_cpp/python_protobuf calls
function(add_protobuf name)
    cmake_parse_arguments(PROTOBUF "" "CPP_TARGET" "" ${ARGN})
    add_cpp_protobuf(${name} ${PROTOBUF_CPP_TARGET} ${PROTOBUF_UNPARSED_ARGUMENTS})
    add_python_protobuf(${name} ${PROTOBUF_UNPARSED_ARGUMENTS})
endfunction()
