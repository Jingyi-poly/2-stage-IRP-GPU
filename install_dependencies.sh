#!/bin/bash

# IRP项目依赖自动安装脚本
# 支持 macOS 和 Ubuntu/Debian

set -e  # 遇到错误时退出

echo "=== IRP项目依赖安装脚本 ==="
echo "检测操作系统..."

# 检测操作系统
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    echo "检测到 macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo "检测到 Linux"
else
    echo "不支持的操作系统: $OSTYPE"
    exit 1
fi

# 检测包管理器
if command -v brew &> /dev/null; then
    PKG_MANAGER="brew"
    echo "使用 Homebrew 包管理器"
elif command -v apt &> /dev/null; then
    PKG_MANAGER="apt"
    echo "使用 apt 包管理器"
else
    echo "未找到支持的包管理器"
    exit 1
fi

# 安装基础开发工具
echo "=== 安装基础开发工具 ==="
if [[ "$OS" == "macos" && "$PKG_MANAGER" == "brew" ]]; then
    brew install gcc make cmake pkg-config
elif [[ "$OS" == "linux" && "$PKG_MANAGER" == "apt" ]]; then
    sudo apt update
    sudo apt install -y build-essential g++ make cmake pkg-config
fi

# 安装gRPC和protobuf
echo "=== 安装 gRPC 和 Protocol Buffers ==="
if [[ "$OS" == "macos" && "$PKG_MANAGER" == "brew" ]]; then
    brew install grpc protobuf
elif [[ "$OS" == "linux" && "$PKG_MANAGER" == "apt" ]]; then
    sudo apt install -y libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
fi

# 安装Python依赖
echo "=== 安装 Python 依赖 ==="
if ! command -v python3 &> /dev/null; then
    echo "Python3 未安装，请先安装 Python3"
    exit 1
fi

# 安装pip (如果需要)
if ! command -v pip3 &> /dev/null; then
    echo "安装 pip3..."
    curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
    python3 get-pip.py --user
    rm get-pip.py
fi

# 安装Python gRPC包
echo "安装 Python gRPC 包..."
pip3 install --user grpcio grpcio-tools protobuf

# 验证安装
echo "=== 验证安装 ==="

# 检查C++依赖
echo "检查 C++ 依赖..."
if pkg-config --exists grpc++; then
    echo "✓ gRPC C++ 库已安装"
else
    echo "✗ gRPC C++ 库未找到"
    exit 1
fi

if pkg-config --exists protobuf; then
    echo "✓ protobuf C++ 库已安装"
else
    echo "✗ protobuf C++ 库未找到"
    exit 1
fi

# 检查Python依赖
echo "检查 Python 依赖..."
if python3 -c "import grpc" 2>/dev/null; then
    echo "✓ Python gRPC 已安装"
else
    echo "✗ Python gRPC 未找到"
    exit 1
fi

if python3 -c "import google.protobuf" 2>/dev/null; then
    echo "✓ Python protobuf 已安装"
else
    echo "✗ Python protobuf 未找到"
    exit 1
fi

# 生成protobuf文件
echo "=== 生成 Protocol Buffers 文件 ==="
cd rpc
if [[ "$OS" == "macos" ]]; then
    protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/opt/homebrew/bin/grpc_cpp_plugin irp.proto
else
    protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` irp.proto
fi
cd ..

# 测试编译
echo "=== 测试编译 ==="
make clean
if make; then
    echo "✓ 编译成功！"
else
    echo "✗ 编译失败，请检查错误信息"
    exit 1
fi

echo ""
echo "=== 安装完成！ ==="
echo "现在可以运行项目了："
echo "1. 启动Python服务器: cd rpc && python3 server.py"
echo "2. 运行C++程序: ./irp Data/Small/Istanze0105h3/0.dat -seed 1000 -type 38 -veh 1 -stock 100"
echo ""
echo "详细使用说明请参考 DEPENDENCIES.md" 