# gRPC集成使用说明

## 概述

本项目实现了C++客户端通过gRPC调用Python服务器的OURemoteSolver逻辑。

## 文件结构

```
rpc/
├── irp.proto              # Protocol Buffers定义文件
├── irp.pb.h               # C++生成的protobuf头文件
├── irp.pb.cc              # C++生成的protobuf源文件
├── irp.grpc.pb.h          # C++生成的gRPC头文件
├── irp.grpc.pb.cc         # C++生成的gRPC源文件
├── irp_pb2.py             # Python生成的protobuf模块
├── irp_pb2_grpc.py        # Python生成的gRPC模块
├── server.py               # Python gRPC服务器
├── client.cpp              # C++ gRPC客户端测试
├── test_grpc.py           # Python gRPC测试脚本
├── start_server.sh         # 启动服务器脚本
└── README_GRPC.md         # 本文件
```

## 安装依赖

### Python依赖
```bash
pip3 install grpcio grpcio-tools protobuf
```

### C++依赖
确保系统已安装gRPC和protobuf：
```bash
# macOS (使用Homebrew)
brew install grpc protobuf

# Ubuntu/Debian
sudo apt-get install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
```

## 使用方法

### 1. 启动Python服务器

```bash
cd rpc
./start_server.sh
```

或者直接运行：
```bash
python3 server.py
```

服务器将在端口50051上启动。

### 2. 编译C++程序

```bash
make clean
make
```

### 3. 运行C++程序

```bash
./irp Data/Small/Istanze0105h3/0.dat -seed 1000 -type 38 -veh 1 -stock 100
```

### 4. 测试gRPC连接

```bash
cd rpc
python3 test_grpc.py
```

## 实现细节

### C++客户端修改

1. **Individu.cpp**: 修改了`RunSearchPi()`函数，将原来的本地OURemoteSolver调用替换为gRPC客户端调用
2. **Makefile**: 添加了gRPC相关的库和头文件路径

### Python服务器实现

1. **server.py**: 实现了完整的OURemoteSolver逻辑，包括：
   - `DayRes`类：对应C++中的DayRes结构
   - `OURemoteSolver`类：实现动态规划算法
   - `IrpServiceServicer`类：处理gRPC请求

### 算法逻辑

Python服务器实现了与C++版本相同的OURemoteSolver逻辑：

1. **动态规划**: 使用记忆化搜索解决库存优化问题
2. **状态转移**: 考虑配送和不配送两种选择
3. **成本计算**: 包括库存成本、缺货成本、配送成本、容量惩罚成本等
4. **回溯**: 根据最优解重建配送计划

## 调试

### 启用调试信息

在`server.py`中，将`traces = False`改为`traces = True`可以启用详细的调试输出。

### 检查连接

使用`test_grpc.py`可以测试gRPC连接是否正常工作。

## 故障排除

1. **编译错误**: 确保已安装gRPC和protobuf库
2. **连接错误**: 确保Python服务器正在运行
3. **协议错误**: 确保proto文件定义正确

## 性能优化

1. **连接池**: 可以添加连接池来复用gRPC连接
2. **异步调用**: 可以实现异步gRPC调用来提高性能
3. **缓存**: 可以在服务器端添加结果缓存 