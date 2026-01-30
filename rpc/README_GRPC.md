# gRPC Integration Guide

## Overview

This project implements C++ client calling Python server's OURemoteSolver logic via gRPC.

## File Structure

```
rpc/
├── irp.proto       # Protocol Buffers definition file
├── irp.pb.h        # C++ generated protobuf header
├── irp.pb.cc       # C++ generated protobuf source
├── irp.grpc.pb.h     # C++ generated gRPC header
├── irp.grpc.pb.cc     # C++ generated gRPC source
├── irp_pb2.py       # Python generated protobuf module
├── irp_pb2_grpc.py    # Python generated gRPC module
├── server.py       # Python gRPC server
├── client.cpp       # C++ gRPC client test
├── test_grpc.py      # Python gRPC test script
├── start_server.sh    # Server startup script
└── README_GRPC.md     # This file
```

## Dependencies

### Python Dependencies
```bash
pip3 install grpcio grpcio-tools protobuf
```

### C++ Dependencies
Ensure gRPC and protobuf are installed:
```bash
# macOS (using Homebrew)
brew install grpc protobuf

# Ubuntu/Debian
sudo apt-get install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
```

## Usage

### 1. Start Python Server

```bash
cd rpc
./start_server.sh
```

Or run directly:
```bash
python3 server.py
```

The server will start on port 50051.

### 2. Compile C++ Program

```bash
make clean
make
```

### 3. Run C++ Program

```bash
./irp Data/Small/Istanze0105h3/0.dat -seed 1000 -type 38 -veh 1 -stock 100
```

### 4. Test gRPC Connection

```bash
cd rpc
python3 test_grpc.py
```

## Implementation Details

### C++ Client Modifications

1. **Individu.cpp**: Modified `RunSearchPi()` function, replacing local OURemoteSolver calls with gRPC client calls
2. **Makefile**: Added gRPC related libraries and include paths

### Python Server Implementation

1. **server.py**: Implements complete OURemoteSolver logic, including:
  - `DayRes` class: Corresponds to C++ DayRes structure
  - `OURemoteSolver` class: Implements dynamic programming algorithm
  - `IrpServiceServicer` class: Handles gRPC requests

### Algorithm Logic

The Python server implements the same OURemoteSolver logic as C++ version:

1. **Dynamic Programming**: Uses memoization to solve inventory optimization
2. **State Transition**: Considers both delivery and no-delivery options
3. **Cost Calculation**: Includes inventory cost, stockout cost, delivery cost, capacity penalty cost
4. **Backtracking**: Reconstructs delivery plan from optimal solution

## Debugging

### Enable Debug Output

In `server.py`, change `traces = False` to `traces = True` for detailed debug output.

### Check Connection

Use `test_grpc.py` to test if gRPC connection is working properly.

## Troubleshooting

1. **Compilation Error**: Ensure gRPC and protobuf libraries are installed
2. **Connection Error**: Ensure Python server is running
3. **Protocol Error**: Ensure proto file definitions are correct

## Advanced Features

1. **Connection Pool**: Add connection pooling to reuse gRPC connections
2. **Async Calls**: Implement async gRPC calls for non-blocking operations
3. **Caching**: Add result caching on server side
