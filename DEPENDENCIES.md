# IRP项目依赖安装指南

## 项目概述

这是一个库存路径规划(Inventory Routing Problem, IRP)的C++项目，集成了gRPC通信功能，支持C++客户端与Python服务器之间的远程调用。

## 系统要求

- **操作系统**: macOS, Linux (Ubuntu/Debian推荐)
- **编译器**: GCC 7.0+ 或 Clang 5.0+
- **Python**: Python 3.7+
- **包管理器**: Homebrew (macOS) 或 apt (Ubuntu/Debian)

## 依赖包安装

### 1. 基础开发工具

#### macOS (使用Homebrew)
```bash
# 安装Homebrew (如果未安装)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装基础开发工具
brew install gcc make cmake pkg-config
```

#### Ubuntu/Debian
```bash
# 更新包列表
sudo apt update

# 安装基础开发工具
sudo apt install build-essential g++ make cmake pkg-config
```

### 2. gRPC和Protocol Buffers

#### macOS
```bash
# 安装gRPC和protobuf
brew install grpc protobuf

# 验证安装
pkg-config --modversion grpc++
pkg-config --modversion protobuf
```

#### Ubuntu/Debian
```bash
# 安装gRPC和protobuf
sudo apt install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
# Install dependencies for grpc
sudo apt-get install libabsl-dev


# 验证安装
pkg-config --modversion grpc++
pkg-config --modversion protobuf
```

### 3. Python依赖

```bash
# 安装Python包管理器 (如果未安装)
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python3 get-pip.py

# 安装gRPC Python包
pip3 install grpcio grpcio-tools protobuf

# 验证安装
python3 -c "import grpc; print('gRPC installed successfully')"
python3 -c "import google.protobuf; print('protobuf installed successfully')"
```

### 4. 可选依赖 (用于调试和开发)

#### macOS
```bash
# 安装调试工具
brew install valgrind gdb

# 安装代码格式化工具
brew install clang-format
```

#### Ubuntu/Debian
```bash
# 安装调试工具
sudo apt install valgrind gdb

# 安装代码格式化工具
sudo apt install clang-format
```

## 验证安装

### 1. 检查C++依赖
```bash
# 检查gRPC库
pkg-config --cflags grpc++
pkg-config --libs grpc++

# 检查protobuf库
pkg-config --cflags protobuf
pkg-config --libs protobuf
```

### 2. 检查Python依赖
```bash
# 检查Python gRPC
python3 -c "import grpc; print('gRPC version:', grpc.__version__)"

# 检查protobuf
python3 -c "import google.protobuf; print('protobuf installed')"
```

### 3. 编译测试
```bash
# 清理之前的编译
make clean

# 编译项目
make

# 如果编译成功，说明依赖安装正确
```

## 项目结构说明

```
IRP-heur-01.21.01/
├── Makefile              # 主编译文件
├── rpc/                  # gRPC相关文件
│   ├── irp.proto        # Protocol Buffers定义
│   ├── server.py         # Python gRPC服务器
│   ├── client.cpp        # C++ gRPC客户端测试
│   └── Makefile          # RPC模块编译文件
├── Data/                 # 测试数据
└── *.cpp *.h            # 主要C++源文件
```

## 使用方法

### 1. 启动Python服务器
```bash
cd rpc
mkdir build && cd build
cmake ..
make
cd ..
python3 server.py
```

### 2. 编译C++程序
```bash
mkdir build && cd build
cmake ..
make
cd ..
```

### 3. 运行程序
```bash
测试可以在主目录下运行：
./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 1000 -scenario 1 -control_day_1 1
或者多场景：
./irp Data/Small/Istanze0105h3/0.dat -seed 989 -type 38 -veh 1 -stock 100 -demandseed 989 -iter 1000 -scenario 2 -control_day_1 1
```

```bash
./irp Data/Small/Istanze0105h3/0.dat -seed 1000 -type 38 -veh 1 -stock 100
```

## 故障排除

### 常见问题

1. **编译错误: gRPC库未找到**
   ```bash
   # 检查gRPC是否正确安装
   pkg-config --cflags grpc++
   # 如果失败，重新安装gRPC
   brew reinstall grpc  # macOS
   sudo apt reinstall libgrpc++-dev  # Ubuntu
   ```

2. **Python gRPC导入错误**
   ```bash
   # 重新安装Python gRPC
   pip3 uninstall grpcio grpcio-tools
   pip3 install grpcio grpcio-tools
   ```

3. **protobuf编译错误**
   ```bash
   # 重新生成protobuf文件
   cd rpc
   protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/opt/homebrew/bin/grpc_cpp_plugin irp.proto
   ```

4. **链接错误**
   ```bash
   # 检查库路径
   echo $LD_LIBRARY_PATH
   # 添加库路径 (如果需要)
   export LD_LIBRARY_PATH=/opt/homebrew/lib:$LD_LIBRARY_PATH
   ```

### 版本兼容性

- **gRPC**: 1.40.0+
- **protobuf**: 3.19.0+
- **Python**: 3.7+
- **GCC**: 7.0+

## 性能优化建议

1. **编译优化**: 在Makefile中使用 `-O2` 或 `-O3` 优化标志
2. **链接优化**: 使用静态链接减少运行时依赖
3. **内存管理**: 项目使用智能指针，无需手动内存管理

## 开发环境配置

### VSCode配置
```json
{
    "C_Cpp.default.compilerPath": "/usr/bin/g++",
    "C_Cpp.default.cStandard": "c17",
    "C_Cpp.default.cppStandard": "c++17"
}
```

### 调试配置
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug IRP",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/irp",
            "args": ["Data/Small/Istanze0105h3/0.dat", "-seed", "1000", "-type", "38", "-veh", "1", "-stock", "100"],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb"
        }
    ]
}
```

## 许可证

本项目遵循相应的开源许可证。请查看项目根目录的LICENSE文件了解详细信息。 