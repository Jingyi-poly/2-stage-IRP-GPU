#!/bin/bash
# GPU Server Startup Script

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR"

# Check Python
if ! command -v python3 &> /dev/null; then
    echo "[ERROR] python3 not found"
    exit 1
fi

# Check dependencies
echo "Checking dependencies..."
python3 -c "import grpc" 2>/dev/null || { echo "[ERROR] grpcio not installed"; exit 1; }
python3 -c "import torch" 2>/dev/null || { echo "[WARNING] PyTorch not installed, GPU acceleration unavailable"; }

# Check GPU
if python3 -c "import torch; exit(0 if torch.cuda.is_available() else 1)" 2>/dev/null; then
    echo "[OK] CUDA available"
    GPU_COUNT=$(python3 -c "import torch; print(torch.cuda.device_count())")
    echo "     GPU count: $GPU_COUNT"
else
    echo "[WARNING] CUDA not available, using CPU mode"
fi

echo ""
echo "Starting GPU server on port 50051..."
echo "Press Ctrl+C to stop"
echo ""

python3 server.py
