# GPU-based Split algorithm for Large-Scale CVRPSD

## Introduction

This is the GPU-based implementation of the split algorithm for CVRPSD. 
To compile, please make sure the settings for CUDA is correct in the CMake file.
Then you can use standard c++ compliling procedure:

```
mkdir build
cd build
cmake ..
make -j
```

To run the code, you can find command templates as follows:
1. CPU:
```
./hgs ../Instances/CVRP/X-n106-k14.vrp mySolution.sol -seed 1 -t 30 -nthreads 8 -nextrascen 4999 -freqPrint 1 -iterLim 1
```
2. GPU:
```
./hgs_cuda ../Instances/CVRP/X-n106-k14.vrp mySolution.sol -seed 1 -t 30 -nthreads 8 -nextrascen 4999 -freqPrint 1 -iterLim 1
```
