cmake_minimum_required(VERSION 3.15)
project(grpc_client CXX)

# Setcan/mayExecuteFileOutputtoDirectory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Set C++ 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# findDepends onpackage
find_package(PkgConfig REQUIRED)

# onlyCheck gRPC and protobuf library
pkg_check_modules(GRPC REQUIRED grpc++ grpc protobuf)

# SetCompile Options
add_compile_options(
  -Wall
  -Wextra
  ${GRPC_CFLAGS_OTHER}
)

# modify1:SetOutputDirectoryforDirectory(with build same)
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# addcustomcommandGenerate gRPC Generation
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/irp.proto)
set(GENERATED_PROTO_FILES
  ${PROTO_OUTPUT_DIR}/irp.pb.cc
  ${PROTO_OUTPUT_DIR}/irp.pb.h
  ${PROTO_OUTPUT_DIR}/irp.grpc.pb.cc
  ${PROTO_OUTPUT_DIR}/irp.grpc.pb.h
)

# find protoc and grpc_cpp_plugin
find_program(PROTOC protoc)
if(NOT PROTOC)
  message(FATAL_ERROR "Protobuf compiler (protoc) not found. Please install protobuf-compiler")
else()
  message(STATUS "Found protoc: ${PROTOC}")
endif()

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
  find_program(GRPC_CPP_PLUGIN 
    NAMES grpc_cpp_plugin
    PATHS /usr/bin /usr/local/bin /opt/homebrew/bin
    DOC "gRPC C++ plugin"
  )
  if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "gRPC C++ plugin (grpc_cpp_plugin) not found. Please install grpc-dev")
  endif()
endif()
message(STATUS "Found grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

# modify2:addGeneratecommand(not needCreateDirectory,directOutputtoDirectory)
add_custom_command(
  OUTPUT ${GENERATED_PROTO_FILES}
  COMMAND ${PROTOC}
    --experimental_allow_proto3_optional
    --cpp_out=${PROTO_OUTPUT_DIR}
    --grpc_out=${PROTO_OUTPUT_DIR}
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    -I ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating gRPC code directly to project root: ${PROTO_OUTPUT_DIR}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Createcan/mayExecuteFile
add_executable(client
  client.cpp
  ${GENERATED_PROTO_FILES}
)

# SetcontainDirectory - Directoryalreadycontain,not needextraadd
target_include_directories(client PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR} # containDirectory
  ${GRPC_INCLUDE_DIRS}
)

# linkinglibrary
target_link_libraries(client PRIVATE
  ${GRPC_LIBRARIES}
  absl_strings
  absl_synchronization
  absl_time
  absl_cord
  pthread
  dl
)

# addcustomtoconvenientGenerate gRPC Generation
add_custom_target(proto
  DEPENDS ${GENERATED_PROTO_FILES}
  COMMENT "Generate gRPC code directly to project root"
)

# addDepends onrelationship gRPC Generationincompilationbefore/frontGenerate
add_dependencies(client proto)

# modify3:UpdateClean Targets,removeGenerateFile
add_custom_target(distclean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/client
 # cleanupprotobufGenerateFile(inDirectory)
  COMMAND ${CMAKE_COMMAND} -E remove 
    ${PROTO_OUTPUT_DIR}/irp.pb.cc
    ${PROTO_OUTPUT_DIR}/irp.pb.h
    ${PROTO_OUTPUT_DIR}/irp.grpc.pb.cc
    ${PROTO_OUTPUT_DIR}/irp.grpc.pb.h
 COMMENT "Completely clean build files,can/mayExecuteFileandGenerateprotobufFile"
)

# addDebuginformation
message(STATUS "GRPC_INCLUDE_DIRS: ${GRPC_INCLUDE_DIRS}")
message(STATUS "GRPC_LIBRARIES: ${GRPC_LIBRARIES}")
message(STATUS "Proto files will be generated directly to project root: ${PROTO_OUTPUT_DIR}")