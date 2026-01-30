#!/bin/bash
# IRP Project Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "============================================"
echo "IRP - Inventory Routing Problem Solver"
echo "Build Script"
echo "============================================"

cd "$PROJECT_ROOT"

# Clean old build
if [ "$1" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf build
    rm -f irp
    echo "Clean completed!"
    exit 0
fi

# Create build directory
mkdir -p build
cd build

# Configure
echo ""
echo "[Step 1/2] CMake configuration..."
cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE:-Release}

# Compile
echo ""
echo "[Step 2/2] Compiling..."
make -j$(nproc)

echo ""
echo "============================================"
echo "Build successful!"
echo "Executable: $PROJECT_ROOT/irp"
echo "============================================"
