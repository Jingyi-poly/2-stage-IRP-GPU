# IRP Module Usage Guide

## Quick Start

### 1. Running from Command Line

```bash
# Standard run
./irp Data/Small/Istanze0105h3/abs4n5_2.dat -seed 1000 -type 38 -veh 3 -stock 1000000

# Parameter description
PATH    - Path to problem instance file
-seed INT - Random number seed
-type INT - Algorithm type, typically 38
-veh INT  - Number of vehicles
-stock INT - Stockout penalty coefficient
-scenario INT - Number of demand scenarios
-threads INT - Number of threads (0 for gRPC mode)
-control_day_1 INT - First-day control mode (0: off, 1: semi-control)
-force_delivery_clients LIST - Comma-separated client indices (1=first customer) for forced day-1 delivery
-t INT   - Time limit in seconds
```

### 2. Parameter List Module

Supports multi-instance batch processing with path lists and scenario counts (see scripts/run_scenarios.sh)

### 3. Module Usage

#### Utils Library
```cpp
#include "irp/utils/Rng.h"

Rng rng(42);
int val = rng.next(100);
```

#### Model Data Structures
```cpp
#include "irp/model/Params.h"

Params params("path/to/instance.dat", "output.sol", 38, 3, "bks.txt", 42, 0, false);
int numClients = params.nbClients;
```

#### Solver Module
```cpp
#include "irp/solver/LotSizingSolver.h"

LotSizingSolver solver(&params, client, ...);
bool success = solver.solve();
double optimalCost = solver.getCost();
```

#### Core Genetic Algorithm
```cpp
#include "irp/core/Genetic.h"

Population pop(&params);
Genetic ga(&params, &pop, timeLimit, true, true);
ga.evolve(10000000, 200000, 1);

pop.ExportPop("output.sol", true);
pop.ExportBKS("bks.txt");
```

#### RPC Client Module

The RPC module enables distributed computing by offloading inventory optimization to a remote GPU server.

**Basic Usage:**

```cpp
#include <grpcpp/grpcpp.h>
#include "rpc/irp.grpc.pb.h"

// Create gRPC channel
std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
    "localhost:50051",
    grpc::InsecureChannelCredentials()
);

// Create client stub
std::unique_ptr<IrpService::Stub> stub = IrpService::NewStub(channel);

// Prepare request
OURequest request;
request.set_client_id(clientId);
request.set_num_days(numDays);
request.set_init_inventory(initInv);
request.set_max_inventory(maxInv);
request.set_holding_cost(holdingCost);
request.set_stockout_cost(stockoutCost);
request.set_delivery_cost(deliveryCost);
request.set_capacity_penalty(capacityPenalty);
request.set_vehicle_capacity(vehicleCapacity);

// Add demands
for (int d = 0; d < numDays; d++) {
    request.add_demands(demands[d]);
}

// Call remote solver
OUResponse response;
grpc::ClientContext context;
grpc::Status status = stub->SolveOU(&context, request, &response);

// Handle response
if (status.ok() && response.success()) {
    double totalCost = response.total_cost();

    // Extract delivery plan
    std::vector<double> deliveries;
    for (int d = 0; d < response.deliveries_size(); d++) {
        deliveries.push_back(response.deliveries(d));
    }

    // Extract inventory levels
    std::vector<double> inventories;
    for (int d = 0; d < response.inventories_size(); d++) {
        inventories.push_back(response.inventories(d));
    }
} else {
    // Fallback to local solver
    std::cerr << "RPC failed: " << status.error_message() << std::endl;
}
```

**Integration in Genetic Algorithm:**

The RPC client is automatically used in `Individu::RunSearchPi()` when `-threads 0` is specified:

```cpp
// In src/core/Individu.cpp
void Individu::RunSearchPi(Params* params) {
    if (params->useGRPC) {  // Triggered by -threads 0
        // Use gRPC client
        auto channel = grpc::CreateChannel("localhost:50051",
                                          grpc::InsecureChannelCredentials());
        auto stub = IrpService::NewStub(channel);

        // For each client, solve via RPC
        for (int c = 0; c < params->nbClients; c++) {
            OURequest request = buildRequest(c);
            OUResponse response;
            grpc::ClientContext context;

            grpc::Status status = stub->SolveOU(&context, request, &response);

            if (status.ok()) {
                applyDeliveryPlan(c, response);
            } else {
                // Fallback to local OUPolicySolver
                localSolve(c);
            }
        }
    } else {
        // Use local CPU solver
        localSolve();
    }
}
```

**Connection Management:**

Reuse gRPC channels across multiple calls:

```cpp
class RPCClientPool {
private:
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<IrpService::Stub> stub;

public:
    RPCClientPool(const std::string& server_address) {
        channel = grpc::CreateChannel(server_address,
                                     grpc::InsecureChannelCredentials());
        stub = IrpService::NewStub(channel);
    }

    bool solveOU(const OURequest& request, OUResponse& response) {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::seconds(10));  // 10s timeout

        grpc::Status status = stub->SolveOU(&context, request, &response);
        return status.ok() && response.success();
    }
};

// Usage
RPCClientPool pool("localhost:50051");
OUResponse response;
if (pool.solveOU(request, response)) {
    // Process response
}
```

**Error Handling Best Practices:**

```cpp
grpc::Status status = stub->SolveOU(&context, request, &response);

if (!status.ok()) {
    switch (status.error_code()) {
        case grpc::StatusCode::UNAVAILABLE:
            std::cerr << "Server unavailable, using local solver" << std::endl;
            break;
        case grpc::StatusCode::DEADLINE_EXCEEDED:
            std::cerr << "RPC timeout, using local solver" << std::endl;
            break;
        case grpc::StatusCode::INVALID_ARGUMENT:
            std::cerr << "Invalid request: " << status.error_message() << std::endl;
            break;
        default:
            std::cerr << "RPC error: " << status.error_message() << std::endl;
    }
    // Always have fallback
    localSolve();
}
```

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc) # Parallel compilation
```

## Testing

```bash
cd build
cmake --build . --target irp
./irp ../Data/Small/Istanze0105h3/0.dat -seed 42 -type 38 -veh 1 -stock 100
```
