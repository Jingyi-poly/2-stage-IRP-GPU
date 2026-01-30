#!/bin/bash

# IRP Project Dependency Installation Script
# Supports macOS and Ubuntu/Debian

set -e  # Exit on error

echo "=== IRP Project Dependency Installation Script ==="
echo "Detecting operating system..."

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    echo "Detected macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo "Detected Linux"
else
    echo "Unsupported operating system: $OSTYPE"
    exit 1
fi

# Detect package manager
if command -v brew &> /dev/null; then
    PKG_MANAGER="brew"
    echo "Using Homebrew package manager"
elif command -v apt &> /dev/null; then
    PKG_MANAGER="apt"
    echo "Using apt package manager"
else
    echo "No supported package manager found"
    exit 1
fi

# Install basic development tools
echo "=== Installing basic development tools ==="
if [[ "$OS" == "macos" && "$PKG_MANAGER" == "brew" ]]; then
    brew install gcc make cmake pkg-config
elif [[ "$OS" == "linux" && "$PKG_MANAGER" == "apt" ]]; then
    sudo apt update
    sudo apt install -y build-essential g++ make cmake pkg-config
fi

# Install gRPC and protobuf
echo "=== Installing gRPC and Protocol Buffers ==="
if [[ "$OS" == "macos" && "$PKG_MANAGER" == "brew" ]]; then
    brew install grpc protobuf
elif [[ "$OS" == "linux" && "$PKG_MANAGER" == "apt" ]]; then
    sudo apt install -y libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
fi

# Install Python dependencies
echo "=== Installing Python dependencies ==="
if ! command -v python3 &> /dev/null; then
    echo "Python3 not installed, please install Python3 first"
    exit 1
fi

# Install pip if needed
if ! command -v pip3 &> /dev/null; then
    echo "Installing pip3..."
    curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
    python3 get-pip.py --user
    rm get-pip.py
fi

# Install Python gRPC packages
echo "Installing Python gRPC packages..."
pip3 install --user grpcio grpcio-tools protobuf

# Verify installation
echo "=== Verifying installation ==="

# Check C++ dependencies
echo "Checking C++ dependencies..."
if pkg-config --exists grpc++; then
    echo "[OK] gRPC C++ library installed"
else
    echo "[FAIL] gRPC C++ library not found"
    exit 1
fi

if pkg-config --exists protobuf; then
    echo "[OK] protobuf C++ library installed"
else
    echo "[FAIL] protobuf C++ library not found"
    exit 1
fi

# Check Python dependencies
echo "Checking Python dependencies..."
if python3 -c "import grpc" 2>/dev/null; then
    echo "[OK] Python gRPC installed"
else
    echo "[FAIL] Python gRPC not found"
    exit 1
fi

if python3 -c "import google.protobuf" 2>/dev/null; then
    echo "[OK] Python protobuf installed"
else
    echo "[FAIL] Python protobuf not found"
    exit 1
fi

# Generate protobuf files
echo "=== Generating Protocol Buffers files ==="
cd rpc
if [[ "$OS" == "macos" ]]; then
    protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/opt/homebrew/bin/grpc_cpp_plugin irp.proto
else
    protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` irp.proto
fi
cd ..

# Test compilation
echo "=== Testing compilation ==="
make clean
if make; then
    echo "[OK] Compilation successful!"
else
    echo "[FAIL] Compilation failed, please check error messages"
    exit 1
fi

echo ""
echo "=== Installation complete! ==="
echo "You can now run the project:"
echo "1. Start Python server: cd rpc && python3 server.py"
echo "2. Run C++ program: ./irp Data/Small/Istanze0105h3/0.dat -seed 1000 -type 38 -veh 1 -stock 100"
echo ""
echo "See DEPENDENCIES.md for detailed usage instructions"
