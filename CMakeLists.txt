cmake_minimum_required(VERSION 3.10)
project(irp CXX)

# 设置可执行文件输出到项目根目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 查找依赖包
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc protobuf)

# 设置编译选项
add_compile_options(
    -g
    -m64
    -O
    -fPIC
    -fno-strict-aliasing
    -fexceptions
    -DNDEBUG
    -DIL_STD
)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GRPC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/rpc
)

# 源文件列表
set(SOURCES
    Client.cpp
    Vehicle.cpp
    commandline.cpp
    Genetic.cpp
    Individu.cpp
    LocalSearch.cpp
    main.cpp
    Noeud.cpp
    Rng.cpp
    Params.cpp
    ParamsList.cpp
    Population.cpp
    Route.cpp
    Mutations.cpp
    LinearPiece.cpp
    PLFunction.cpp
    LotSizingSolver.cpp
    OUPolicySolver.cpp
    ThreadPool.cpp
    ScenarioUtils.cpp
    rpc/irp.pb.cc
    rpc/irp.grpc.pb.cc
)

# 创建可执行文件
add_executable(irp ${SOURCES})

# 链接库
target_link_libraries(irp
    PRIVATE
    m
    pthread
    dl
    ${GRPC_LDFLAGS}
)

# 添加清理目标
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/irp
    COMMENT "完全清理构建文件和可执行文件"
)