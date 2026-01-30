# OUIRP-GPU: Hybrid Genetic Search for the Inventory Routing Problem

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Python](https://img.shields.io/badge/Python-3.10+-green.svg)](https://www.python.org/)

A solver for the **Inventory Routing Problem (IRP)** with stochastic demand, based on the **Hybrid Genetic Search with Adaptive Diversity Control (HGSADC)** framework.

## Overview

The Inventory Routing Problem combines vehicle routing and inventory management decisions. Given a supplier and a set of customers with stochastic demands, limited inventory capacities, and holding/stockout costs, the goal is to determine:
- **When** to visit each customer
- **How much** to deliver
- **Which route** to follow

This solver implements a **two-stage stochastic optimization** approach that makes robust first-stage decisions under demand uncertainty.

### Key Features

- **Hybrid Genetic Search**: Combines genetic algorithms with local search
- **Two-Stage Stochastic Optimization**: Handles demand uncertainty via scenario-based optimization
- **GPU Acceleration**: Optional CUDA-based parallel inventory optimization via gRPC
- **Distributed Computing**: gRPC interface for offloading computations to remote servers
- **Multi-threading**: Parallel route and inventory optimization

## Project Structure

```
OUIRP-GPU/
├── include/irp/      # Header files
│   ├── core/         # Genetic algorithm (Genetic, Population, Individu)
│   ├── model/        # Data models (Params, Client, Vehicle, Route)
│   ├── solver/       # Inventory solvers (OU-Policy, Lot-Sizing DP)
│   ├── search/       # Local search (2-opt, Relocate, SWAP)
│   └── utils/        # Utilities (RNG, ThreadPool, CLI)
├── src/              # Source implementations
├── rpc/              # gRPC server for GPU acceleration
│   ├── server.py     # Python gRPC server
│   ├── gpu_solver.py # PyTorch-based GPU DP solver
│   └── irp.proto     # Protocol buffer definitions
├── Data/             # Benchmark instances
│   ├── Small/        # Small instances (5-20 customers)
│   └── Big/          # Large instances (50+ customers)
├── docs/             # Documentation
├── examples/         # Usage examples
├── scripts/          # Utility scripts
└── main.cpp          # Main entry point
```

## Requirements

### C++ Dependencies
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10+
- gRPC and Protocol Buffers (optional, for distributed computing)
- pthread

### Python Dependencies (Optional, for GPU acceleration)
- Python 3.10+
- PyTorch (with CUDA support for GPU)
- grpcio, grpcio-tools, protobuf

## Installation

### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd OUIRP-GPU

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# The executable 'irp' will be created in the project root
```

### Installing Python Dependencies (Optional)

```bash
# Using conda (recommended)
conda env create -f environment.yml
conda activate irp-env

# Or using pip
pip install -r requirements.txt
```

## Usage

### Basic Usage

```bash
./irp Data/Small/Istanze0105h3/abs4n5_2.dat -seed 42 -type 38 -veh 3 -stock 1000000
```

### Command Line Arguments

| Argument | Description | Default |
|----------|-------------|---------|
| `<instance>` | Path to problem instance file | Required |
| `-seed <int>` | Random seed for reproducibility | 0 |
| `-type <int>` | Problem type (38 for IRP) | 38 |
| `-veh <int>` | Number of vehicles | 3 |
| `-stock <float>` | Stockout penalty coefficient | 1000000 |
| `-scenario <int>` | Number of demand scenarios | 1 |
| `-threads <int>` | Number of threads (0 for gRPC mode) | auto |
| `-t <int>` | Time limit in seconds | 1200 |
| `-iter <int>` | Max non-improving iterations | 100 |
| `-control_day_1 <int>` | First-day control mode (0: off, 1: semi-control) | 0 |
| `-force_delivery_clients <list>` | Comma-separated client indices for forced day-1 delivery | (none) |

### Multi-Scenario Stochastic Optimization

```bash
# Run with 100 demand scenarios using local CPU
./irp instance.dat -scenario 100 -threads 8

# Run with GPU acceleration (requires gRPC server)
python rpc/server.py &  # Start GPU server
./irp instance.dat -scenario 100 -threads 0

# Run with forced first-day delivery for specific clients
./irp instance.dat -scenario 100 -control_day_1 1 -force_delivery_clients 1,2,3
```

## Algorithm Details

### Two-Phase Local Search

1. **Route Improvement (RI)**: Optimizes vehicle routes using:
   - 2-opt, 2-opt*
   - Relocate, Swap
   - Cross-exchange

2. **Pattern Improvement (PI)**: Optimizes delivery schedules using:
   - Order-Up-To (OU) policy with dynamic programming
   - Lot-sizing with piecewise linear cost functions

### Two-Stage Stochastic Optimization

For uncertain demand, the solver:
1. Generates multiple demand scenarios
2. Optimizes first-day decisions considering all scenarios
3. Optimizes subsequent days per scenario
4. Selects robust solutions minimizing expected cost

## RPC Integration for GPU Acceleration

### Overview

The solver supports **gRPC-based distributed computing** to offload inventory optimization to a GPU-accelerated Python server. This is particularly beneficial for:
- Large-scale instances with many scenarios (100+)
- Problems requiring intensive dynamic programming computations
- Systems with available GPU resources

### Architecture

```
┌─────────────────┐         gRPC          ┌─────────────────┐
│   C++ Client    │ ◄──────────────────► │  Python Server  │
│  (Main Solver)  │   (Port 50051)        │  (GPU Solver)   │
│                 │                       │                 │
│ • Genetic Algo  │                       │ • PyTorch DP    │
│ • Route Search  │                       │ • GPU Parallel  │
│ • Local Search  │                       │ • OUPolicy      │
└─────────────────┘                       └─────────────────┘
```

### Setup

#### 1. Install Python Dependencies

```bash
# Using conda (recommended)
conda env create -f environment.yml
conda activate irp-env

# Or using pip
pip install grpcio grpcio-tools protobuf torch
```

#### 2. Start gRPC Server

```bash
# Start server in background
cd rpc
python server.py &

# Or use the startup script
./start_server.sh
```

The server will listen on `localhost:50051` by default.

#### 3. Run Solver in RPC Mode

```bash
# Use -threads 0 to enable gRPC mode
./irp Data/Small/Istanze0105h3/abs4n5_2.dat -scenario 100 -threads 0 -seed 42
```

### Usage Modes

| Mode | Command | Use Case |
|------|---------|----------|
| **Local CPU** | `-threads 8` | Small instances, no GPU available |
| **RPC GPU** | `-threads 0` | Large instances with GPU available |
| **Auto** | (omit `-threads`) | Automatic selection based on available CPU cores |

### Troubleshooting

**Server Connection Failed:**
```bash
# Check if server is running
ps aux | grep "python.*server.py"

# Check port availability
lsof -i :50051

# Restart server
pkill -f "python.*server.py"
cd rpc && python server.py &
```

**GPU Not Detected:**
```bash
# Verify PyTorch CUDA support
python -c "import torch; print(torch.cuda.is_available())"

# Server will automatically fall back to CPU if GPU unavailable
```

**Protocol Version Mismatch:**
```bash
# Regenerate protocol buffers
cd rpc
python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. irp.proto
```

### Advanced Configuration

**Custom Server Address:**
```cpp
// In src/core/Individu.cpp, modify:
std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
    "your-server:50051", grpc::InsecureChannelCredentials());
```

**Enable Debug Logging:**
```python
# In rpc/server.py, set:
traces = True  # Enables detailed DP trace output
```

For complete RPC implementation details, see [gRPC Setup Guide](rpc/README_GRPC.md).

## Instance Format

The solver reads instances in the following format:

```
<num_nodes> <num_days> <vehicle_capacity>
<depot_x> <depot_y> <init_inv> <max_inv> <holding_cost>
<cust1_x> <cust1_y> <init_inv> <max_inv> <min_inv> <max_demand> <d1> <d2> ... <dn>
...
```

See `Data/Small/` for example instances.

## Documentation

- [Architecture Overview](docs/architecture.md) - Detailed module structure and dependencies
- [Module Guide](docs/module_guide.md) - API usage and code examples
- [gRPC Setup](rpc/README_GRPC.md) - Distributed computing configuration

## Citation

If you use this code in your research, please cite:

```bibtex
@software{ouirp_gpu,
  title={OUIRP-GPU: Hybrid Genetic Search for the Inventory Routing Problem},
  author={Your Name},
  year={2024},
  url={https://github.com/yourusername/OUIRP-GPU}
}
```

## Acknowledgments

This implementation builds upon:
- **HGSADC** by [Thibaut Vidal](https://github.com/vidalt/HGS-CVRP) for the genetic algorithm framework
- **Archetti et al. (2007)** for IRP benchmark instances

## License

This project is licensed under the MIT License. See the LICENSE file for details.

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Make your changes and test thoroughly
4. Commit with clear messages
5. Push and create a Pull Request

Please ensure:
- Code compiles without warnings
- Follows existing code style (C++17 standard)
- Includes comments for complex logic
- Tests pass on your local machine

## Contact

For questions, bug reports, or feature requests, please open an issue in the repository.
