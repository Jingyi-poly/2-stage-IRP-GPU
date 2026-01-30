# IRP Architecture Documentation

## Module Structure

```
Main (main.cpp)
 │
 └─── Core Module (irp_core)
    │  Genetic.cpp - Genetic algorithm main control
    │  Population.cpp - Population management
    │  Individu.cpp - Individual representation
    │  Mutations.cpp - Mutation operations
    │
    ├─── Search Module (irp_search)
    │   │  LocalSearch.cpp - Local search
    │   │
    │   └─── Solver Module (irp_solver)
    │      │  LotSizingSolver.cpp - Lot-sizing solver
    │      │  OUPolicySolver.cpp - Order-Up-To policy solver
    │      │  PLFunction.cpp - Piecewise linear function
    │      │  LinearPiece.cpp - Linear piece
    │      │
    │      └─── Model Module (irp_model)
    │         │  Params.cpp - Problem parameters
    │         │  Client.cpp - Client data
    │         │  Vehicle.cpp - Vehicle data
    │         │  Route.cpp - Route data
    │         │  Noeud.cpp - Node data
    │         │
    │         └─── Utils Module (irp_utils)
    │            Rng.cpp - Random number generation
    │            ThreadPool.cpp - Thread pool
    │            ScenarioUtils.cpp - Scenario utilities
    │            EnhancedDemandGenerator.cpp - Demand generation
    │            commandline.cpp - Command line parsing
    │
    └─── RPC Module (irp_rpc)
       │  irp.pb.cc / irp.grpc.pb.cc - C++ gRPC client stubs
       │  irp_pb2.py / irp_pb2_grpc.py - Python gRPC server stubs
       │
       └─── Python Server (rpc/)
          server.py - gRPC service implementation
          gpu_solver.py - GPU-accelerated DP solver
```

## Module Description

### Utils (Utility Library)
**Dependencies**: None

General utilities and helper functions providing shared resources for other modules.

- `Rng` - Random number generator with reproducible sequences
- `ThreadPool` - Thread pool implementation for parallel task execution
- `ScenarioUtils` - Scenario generation and management tools
- `EnhancedDemandGenerator` - Enhanced demand generator
- `commandline` - Command line argument parser

### Model (Data Model)
**Dependencies**: Utils

Problem representation for instances. Contains all data structures directly used by algorithms.

- `Params` - Core header file containing problem parameters, client list, route data
- `Client` - Customer and depot inventory, demand parameters
- `Vehicle` - Vehicle capacity and route management
- `Route` - Route representation
- `Noeud` (Node) - Nodes in routes
- `ParamsList` - Parameter list management for multi-scenario support

### Solver (Solver)
**Dependencies**: Model

Dynamic programming solvers for inventory optimization. Contains piecewise linear programming and DP algorithms.

- `LotSizingSolver` - Lot-sizing solver for batch ordering, inventory, and stockout decisions
- `OUPolicySolver` - Order-Up-To policy solver supporting various modes
- `PLFunction` - Piecewise linear function representation
- `LinearPiece` - Linear segments of functions

### Search (Search)
**Dependencies**: Model, Solver

Local search algorithm implementations. Contains route and inventory operations.

- `LocalSearch` - Route operations (2-opt, relocate, swap) and inventory operations (PI/RI algorithms)

### Core (Core)
**Dependencies**: Model, Solver, Search

Genetic algorithm framework. Manages population evolution and genetic operations.

- `Genetic` - Genetic algorithm main controller
- `Population` - Population management
- `Individu` - Individual representation with chromosome encoding
- `Mutations.cpp` - Mutation operation implementations

### RPC (Remote Procedure Call)
**Dependencies**: Model, Solver (indirect via gRPC protocol)

Distributed computing module enabling GPU-accelerated inventory optimization via gRPC. Provides client-server architecture for offloading computationally intensive dynamic programming to remote Python servers.

#### Architecture Components

**C++ Client Side:**
- `irp.proto` - Protocol Buffers definition file defining service interface
- `irp.pb.h/cc` - Generated protobuf message serialization code
- `irp.grpc.pb.h/cc` - Generated gRPC client stub code
- Integration in `Individu.cpp:RunSearchPi()` - Replaces local solver calls with RPC calls

**Python Server Side:**
- `server.py` - gRPC service implementation and request handler
- `gpu_solver.py` - PyTorch-based GPU-accelerated DP solver
- `irp_pb2.py` - Generated Python protobuf message classes
- `irp_pb2_grpc.py` - Generated Python gRPC server stub

#### Communication Protocol

**Request Message (`OURequest`):**
```protobuf
message OURequest {
  int32 client_id = 1;           // Client identifier
  int32 num_days = 2;            // Planning horizon
  repeated double demands = 3;    // Demand per day
  double init_inventory = 4;      // Initial inventory level
  double max_inventory = 5;       // Maximum inventory capacity
  double holding_cost = 6;        // Per-unit holding cost
  double stockout_cost = 7;       // Per-unit stockout penalty
  double delivery_cost = 8;       // Fixed delivery cost
  double capacity_penalty = 9;    // Capacity violation penalty
  int32 vehicle_capacity = 10;    // Vehicle capacity constraint
}
```

**Response Message (`OUResponse`):**
```protobuf
message OUResponse {
  double total_cost = 1;          // Optimal total cost
  repeated double deliveries = 2; // Delivery quantities per day
  repeated double inventories = 3;// Inventory levels per day
  bool success = 4;               // Solver success flag
}
```

#### Execution Flow

1. **Client Request**: C++ solver calls `RunSearchPi()` during pattern improvement
2. **Serialization**: Problem data serialized to protobuf format
3. **Network Transfer**: gRPC sends request to Python server (default: localhost:50051)
4. **Server Processing**:
   - Deserializes request
   - Executes GPU-accelerated dynamic programming
   - Computes optimal delivery policy
5. **Response**: Server serializes solution and sends back to client
6. **Deserialization**: C++ client extracts solution and continues optimization

#### Design Characteristics

**Advantages:**
- **GPU Acceleration**: Leverages GPU for parallel dynamic programming computations
- **Parallel Processing**: PyTorch batches multiple DP computations
- **Memory Efficiency**: Offloads memory-intensive computations from C++ heap
- **Flexibility**: Easy to swap solver implementations without recompiling C++

**Trade-offs:**
- **Network Communication**: Requires network round-trip for each RPC call
- **Serialization**: Protobuf encoding/decoding required
- **Setup Complexity**: Requires Python environment and server management

**Use Cases:**
- Large-scale instances with many customers
- Multi-scenario stochastic optimization
- Long planning horizons
- Systems with available GPU resources

#### Fallback Mechanism

The solver automatically falls back to local CPU computation if:
- gRPC server is unavailable (connection timeout)
- Server returns error status
- User specifies `-threads > 0` (explicit local mode)

## Dependency Rules

1. **Dependency Direction**: Utils → Model → Solver → Search → Core
2. **Module Encapsulation**: Each module provides shared resources with clear responsibilities
3. **Header/Source Separation**: Public headers in `include/irp/<module>/`, sources in `src/<module>/`

## Public Header Structure

```
include/irp/
 ├── core/
 │  ├── Genetic.h    # Genetic algorithm
 │  ├── Population.h  # Population management
 │  └── Individu.h   # Individual representation
 ├── model/
 │  ├── Params.h    # Problem parameters
 │  ├── Client.h    # Client data
 │  ├── Vehicle.h    # Vehicle data
 │  ├── Route.h     # Route data
 │  ├── Noeud.h     # Node data
 │  └── ParamsList.h  # Parameter list
 ├── solver/
 │  ├── LotSizingSolver.h  # Lot-sizing solver
 │  ├── OUPolicySolver.h  # OU policy solver
 │  ├── PLFunction.h    # Piecewise linear function
 │  ├── LinearPiece.h    # Linear piece
 │  └── Model_LotSizingPI.h # Model definitions
 ├── search/
 │  └── LocalSearch.h    # Local search
 └── utils/
   ├── Rng.h       # Random number generation
   ├── ThreadPool.h    # Thread pool
   ├── ScenarioUtils.h  # Scenario utilities
   ├── EnhancedDemandGenerator.h # Demand generation
   └── commandline.h   # Command line parsing
```
