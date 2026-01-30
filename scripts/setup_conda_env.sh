#!/bin/bash

# Conda Environment Setup Script for IRP Project

set -e

ENV_NAME="irp-gpu"

echo "=== IRP Conda Environment Setup ==="
echo ""

# Check if conda is installed
if ! command -v conda &> /dev/null; then
    echo "[FAIL] conda not found, please install Anaconda or Miniconda first"
    exit 1
fi

# Check if environment already exists
if conda env list | grep -q "^$ENV_NAME "; then
    echo "[INFO] Environment '$ENV_NAME' already exists"
    read -p "Do you want to remove and recreate it? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Removing existing environment..."
        conda env remove -n $ENV_NAME -y
    else
        echo "Keeping existing environment"
        exit 0
    fi
fi

# Create new environment
echo "[INFO] Creating conda environment '$ENV_NAME'..."
conda create -n $ENV_NAME python=3.10 -y

# Activate environment
echo "[INFO] Activating environment..."
source $(conda info --base)/etc/profile.d/conda.sh
conda activate $ENV_NAME

# Install PyTorch with CUDA support
echo "[INFO] Installing PyTorch with CUDA support..."
conda install pytorch torchvision torchaudio pytorch-cuda=11.8 -c pytorch -c nvidia -y

# Install gRPC
echo "[INFO] Installing gRPC..."
pip install grpcio grpcio-tools protobuf

# Verify installation
echo ""
echo "=== Verifying installation ==="
python -c "import torch; print(f'PyTorch: {torch.__version__}, CUDA: {torch.cuda.is_available()}')"
python -c "import grpc; print(f'gRPC: {grpc.__version__}')"

echo ""
echo "=== Setup complete! ==="
echo ""
echo "To activate the environment, run:"
echo "    conda activate $ENV_NAME"
echo ""
echo "To start the GPU server:"
echo "    cd rpc && python server.py"
