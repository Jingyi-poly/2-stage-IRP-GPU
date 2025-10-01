cmake_minimum_required(VERSION 3.15)
project(grpc_client CXX)

# 设置可执行文件输出到项目根目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找依赖包
find_package(PkgConfig REQUIRED)

# 只检查核心 gRPC 和 protobuf 库
pkg_check_modules(GRPC REQUIRED grpc++ grpc protobuf)

# 设置编译选项
add_compile_options(
    -Wall
    -Wextra
    ${GRPC_CFLAGS_OTHER}
)

# 修改点1：设置输出目录为项目根目录（与 build 同级）
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 添加自定义命令生成 gRPC 代码
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/irp.proto)
set(GENERATED_PROTO_FILES
    ${PROTO_OUTPUT_DIR}/irp.pb.cc
    ${PROTO_OUTPUT_DIR}/irp.pb.h
    ${PROTO_OUTPUT_DIR}/irp.grpc.pb.cc
    ${PROTO_OUTPUT_DIR}/irp.grpc.pb.h
)

# 查找 protoc 和 grpc_cpp_plugin
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

# 修改点2：添加生成命令（不需要创建目录，直接输出到根目录）
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

# 创建可执行文件
add_executable(client
    client.cpp
    ${GENERATED_PROTO_FILES}
)

# 设置包含目录 - 项目根目录已经包含，不需要额外添加
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # 包含项目根目录
    ${GRPC_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(client PRIVATE
    ${GRPC_LIBRARIES}
    absl_strings
    absl_synchronization
    absl_time
    absl_cord
    pthread
    dl
)

# 添加自定义目标以方便生成 gRPC 代码
add_custom_target(proto
    DEPENDS ${GENERATED_PROTO_FILES}
    COMMENT "Generate gRPC code directly to project root"
)

# 添加依赖关系确保 gRPC 代码在编译前生成
add_dependencies(client proto)

# 修改点3：更新清理目标，移除生成的文件
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/client
    # 清理protobuf生成的文件（在项目根目录）
    COMMAND ${CMAKE_COMMAND} -E remove 
        ${PROTO_OUTPUT_DIR}/irp.pb.cc
        ${PROTO_OUTPUT_DIR}/irp.pb.h
        ${PROTO_OUTPUT_DIR}/irp.grpc.pb.cc
        ${PROTO_OUTPUT_DIR}/irp.grpc.pb.h
    COMMENT "完全清理构建文件、可执行文件和生成的protobuf文件"
)

# 添加调试信息
message(STATUS "GRPC_INCLUDE_DIRS: ${GRPC_INCLUDE_DIRS}")
message(STATUS "GRPC_LIBRARIES: ${GRPC_LIBRARIES}")
message(STATUS "Proto files will be generated directly to project root: ${PROTO_OUTPUT_DIR}")