#!/bin/bash

# IRP Project Dependency Check Script

echo "=== IRP Dependency Check ==="
echo ""

# Check C++ compiler
echo "[1] C++ Compiler:"
if command -v g++ &> /dev/null; then
    echo "    [OK] g++ $(g++ --version | head -1)"
else
    echo "    [FAIL] g++ not found"
fi

# Check CMake
echo "[2] CMake:"
if command -v cmake &> /dev/null; then
    echo "    [OK] $(cmake --version | head -1)"
else
    echo "    [FAIL] cmake not found"
fi

# Check gRPC C++
echo "[3] gRPC C++:"
if pkg-config --exists grpc++; then
    echo "    [OK] $(pkg-config --modversion grpc++)"
else
    echo "    [FAIL] gRPC C++ library not found"
fi

# Check protobuf C++
echo "[4] Protobuf C++:"
if pkg-config --exists protobuf; then
    echo "    [OK] $(pkg-config --modversion protobuf)"
else
    echo "    [FAIL] protobuf C++ library not found"
fi

# Check Python
echo "[5] Python:"
if command -v python3 &> /dev/null; then
    echo "    [OK] $(python3 --version)"
else
    echo "    [FAIL] python3 not found"
fi

# Check Python gRPC
echo "[6] Python gRPC:"
if python3 -c "import grpc; print(grpc.__version__)" 2>/dev/null; then
    echo "    [OK] grpcio $(python3 -c 'import grpc; print(grpc.__version__)')"
else
    echo "    [FAIL] Python gRPC not installed"
fi

# Check PyTorch
echo "[7] PyTorch:"
if python3 -c "import torch; print(torch.__version__)" 2>/dev/null; then
    VER=$(python3 -c "import torch; print(torch.__version__)")
    CUDA=$(python3 -c "import torch; print(torch.cuda.is_available())")
    echo "    [OK] PyTorch $VER (CUDA: $CUDA)"
else
    echo "    [WARNING] PyTorch not installed (GPU acceleration unavailable)"
fi

echo ""
echo "=== Check complete ==="
