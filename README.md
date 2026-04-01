# GPU-Accelerated Split Algorithm for Large-Scale Stochastic CVRP

A GPU-accelerated Hybrid Genetic Search (HGS) solver for the Capacitated Vehicle Routing Problem with Stochastic Demands (CVRPSD). Built on top of [HGS-CVRP](https://github.com/vidalt/HGS-CVRP), this project adds CUDA-based parallel Split evaluation across scenarios and supports optional-visit (Prize-Collecting TSP) variants.

## Features

- **CPU solver** (`hgs`) — OpenMP-parallel Split across stochastic scenarios
- **GPU solver** (`hgs_cuda`) — CUDA-batched Split for large-scale scenario evaluation
- **PCTSP exact solver** (`pctsp_exact`) — Gurobi-based Prize-Collecting TSP baseline (deterministic + stochastic)
- **Python baselines** — MIP-based exact CVRP solver (`exact_baseline.py`) and PCTSP solver (`pctsp/`)
- Optional-visit mode with configurable skip penalties

## Prerequisites

- CMake ≥ 3.15
- C++17 compiler with OpenMP support
- CUDA Toolkit (tested with compute capability 7.5)
- *(Optional)* [Gurobi](https://www.gurobi.com/) for the PCTSP exact solver

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

This produces three executables in `build/`:

| Binary        | Description                                 |
|---------------|---------------------------------------------|
| `hgs`         | CPU solver (OpenMP parallel)                |
| `hgs_cuda`    | GPU solver (CUDA batched Split)             |
| `pctsp_exact` | PCTSP exact solver (requires Gurobi)        |

## Usage

### CPU solver

```bash
./hgs <instance.vrp> <solution.sol> [options]
```

### GPU solver

```bash
./hgs_cuda <instance.vrp> <solution.sol> [options]
```

### Examples

```bash
# Quick CPU run (8 threads, 5000 scenarios, 30s time limit)
./hgs ../Instances/CVRP/X-n106-k14.vrp sol.sol -seed 1 -t 30 -nthreads 8 -nextrascen 4999

# GPU run with optional-visit mode
./hgs_cuda ../Instances/CVRP/X-n106-k14.vrp sol.sol -seed 1 -t 300 \
    -nextrascen 99999 -iterLim 100 -optionalVisit 1

# PCTSP exact baseline
./pctsp_exact ../Instances/CVRP/X-n106-k14.vrp sol.sol -seed 1 \
    -nextrascen 99 -timeLim 600
```

### Command-Line Options

**Core:**

| Option              | Type     | Default   | Description                                      |
|---------------------|----------|-----------|--------------------------------------------------|
| `-t`                | double   | 0 (off)   | Time limit in seconds (iterative restarts)       |
| `-it`               | int      | 20000     | Max iterations without improvement               |
| `-iterLim`          | int      | —         | Hard iteration limit                             |
| `-timeLim`          | int      | —         | Wall-clock time limit (seconds)                  |
| `-seed`             | int      | 0         | Random seed                                      |
| `-veh`              | int      | auto      | Prescribed fleet size                            |
| `-round`            | bool     | 1         | Round distances to nearest integer               |
| `-log`              | bool     | 1         | Verbose output                                   |
| `-nthreads`         | int      | 1         | OpenMP thread count                              |

**Stochastic / scenario:**

| Option              | Type     | Default   | Description                                      |
|---------------------|----------|-----------|--------------------------------------------------|
| `-nextrascen`       | int      | 0         | Number of extra demand scenarios to generate     |
| `-gpuBatchSize`     | int      | —         | Batch size for GPU evaluation                    |
| `-deviceId`         | int      | 0         | CUDA device ID                                   |
| `-exportScenarios`  | int      | 0         | Export scenarios to binary file                  |

**Optional-visit / PCTSP:**

| Option              | Type     | Default   | Description                                      |
|---------------------|----------|-----------|--------------------------------------------------|
| `-optionalVisit`    | int      | 0         | Enable optional-visit mode                       |
| `-skipPenScale`     | double   | —         | Scale factor for skip penalties                  |
| `-maxClient`        | int      | —         | Max client count                                 |

**GA tuning:**

| Option                      | Type     | Default | Description                                    |
|-----------------------------|----------|---------|------------------------------------------------|
| `-mu`                       | int      | 25      | Minimum population size                        |
| `-lambda`                   | int      | 40      | Generation size                                |
| `-nbElite`                  | int      | 5       | Number of elite individuals                    |
| `-nbClose`                  | int      | 4       | Neighbors for diversity contribution           |
| `-nbGranular`               | int      | 20      | Granular search parameter                      |
| `-nbIterPenaltyManagement`  | int      | 100     | Iterations between penalty updates             |
| `-targetFeasible`           | double   | 0.2     | Target feasible ratio                          |
| `-penaltyIncrease`          | double   | 1.2     | Penalty increase multiplier                    |
| `-penaltyDecrease`          | double   | 0.85    | Penalty decrease multiplier                    |
| `-freqPrint`                | int      | —       | Print frequency                                |
| `-nbIterTraces`             | int      | 500     | Iterations between trace display               |

## Project Structure

```
Program/              Core C++/CUDA source code
  main2.cpp           CPU solver entry point
  main_cuda.cpp       GPU solver entry point
  main_pctsp.cpp      PCTSP exact solver entry point
  Split.cpp/h         CPU Split algorithm
  SplitCUDA.cu/h      CUDA Split algorithm
  GeneticHGS.cpp/h    HGS genetic algorithm driver
  Genetic.cpp/h       Original HGS genetic algorithm
  LocalSearch.cpp/h   Local search with SWAP* neighborhood
  Individual.cpp/h    Solution representation
  Population.cpp/h    Population management
  Params.cpp/h        Problem data and parameters
  PCTSPExact.cpp/h    Gurobi-based PCTSP solver
  InstanceCVRPLIB.*   CVRPLIB instance parser
  AlgorithmParameters.* Algorithm parameter definitions
  C_Interface.*       C API wrapper
Instances/CVRP/       Benchmark instances (CMT, Golden, Uchoa X)
Test/                 CTest integration tests
baseline/             MIP baseline scripts
exact_baseline.py     Exact CVRP solver (PuLP + CBC)
pctsp/                Python PCTSP solver and analysis
  src/                Gurobi-based PCTSP solver modules
  data/               PCTSP benchmark instances
anpy/                 Analysis and plotting scripts
```

## Output Format

Progress is displayed as:

```
It [N1] [N2] | T(s) [T] | Feas [NF] [BestF] [AvgF] | Inf [NI] [BestI] [AvgI] | Div [DivF] [DivI] | Feas [FeasC] [FeasD] | Pen [PenC] [PenD]
```

- `N1`, `N2` — Total iterations / iterations without improvement
- `T` — Elapsed CPU time
- `NF`, `NI` — Feasible / infeasible subpopulation sizes
- `BestF`, `BestI` — Best feasible / infeasible solution values
- `DivF`, `DivI` — Subpopulation diversity
- `FeasC`, `FeasD` — Naturally feasible ratio (capacity / duration)
- `PenC`, `PenD` — Current penalty levels

## References

[1] Vidal, T., Crainic, T. G., Gendreau, M., Lahrichi, N., Rei, W. (2012).
A hybrid genetic algorithm for multidepot and periodic vehicle routing problems. *Operations Research*, 60(3), 611-624.

[2] Vidal, T. (2022). Hybrid genetic search for the CVRP: Open-source implementation and SWAP* neighborhood. *Computers & Operations Research*, 140, 105643.

## License

[MIT](LICENSE) — Copyright (c) 2020 Thibaut Vidal
