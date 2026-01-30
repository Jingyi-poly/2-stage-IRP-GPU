#!/bin/bash
# IRP Basic Usage Example
#
# Usage:
# ./examples/basic_run.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=================================================="
echo "IRP - Inventory Routing Problem Solver"
echo ""
echo "Build & Run Example"
echo "=================================================="

# Build first
echo ""
echo "[Step 1] Building..."
cd "$PROJECT_ROOT"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -n 5
make -j$(nproc) 2>&1 | tail -n 3
cd "$PROJECT_ROOT"

# Run example
echo ""
echo "[Step 2] Running small instance..."
./irp Data/Small/Istanze0105h3/abs4n5_2.dat \
  -seed 42 \
  -type 38 \
  -veh 3 \
  -stock 1000000

echo ""
echo "[Step 3] Running with forced day-1 delivery..."
./irp Data/Small/Istanze0105h3/abs1n5_1.dat \
  -seed 42 \
  -type 38 \
  -veh 1 \
  -stock 100 \
  -scenario 5 \
  -control_day_1 1 \
  -force_delivery_clients 1,2 \
  -threads 1 \
  -iter 50

echo ""
echo "[Done] Run completed!"
