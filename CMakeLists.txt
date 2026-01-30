cmake_minimum_required(VERSION 3.10)
project(IRP VERSION 1.0 LANGUAGES CXX)

# ============================================================================
# Basic Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# ============================================================================
# Dependency Check
# ============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc protobuf)

# ============================================================================
# Compile Options
# ============================================================================
add_compile_options(
  -g -O -fPIC
  -fno-strict-aliasing
  -fexceptions
  -DNDEBUG -DIL_STD
)

# ============================================================================
# Global Include Directories
# ============================================================================
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/irp
  ${CMAKE_SOURCE_DIR}/include/irp/core
  ${CMAKE_SOURCE_DIR}/include/irp/model
  ${CMAKE_SOURCE_DIR}/include/irp/solver
  ${CMAKE_SOURCE_DIR}/include/irp/search
  ${CMAKE_SOURCE_DIR}/include/irp/utils
  ${GRPC_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/rpc
)

# ============================================================================
# Source File Definitions
# ============================================================================

# Utils Module - Utility Library
set(UTILS_SOURCES
  src/utils/Rng.cpp
  src/utils/ThreadPool.cpp
  src/utils/ScenarioUtils.cpp
  src/utils/EnhancedDemandGenerator.cpp
  src/utils/commandline.cpp
)

# Model Module - Data Models
set(MODEL_SOURCES
  src/model/Client.cpp
  src/model/Vehicle.cpp
  src/model/Noeud.cpp
  src/model/Route.cpp
  src/model/Params.cpp
  src/model/ParamsList.cpp
)

# Solver Module - Solvers
set(SOLVER_SOURCES
  src/solver/LinearPiece.cpp
  src/solver/PLFunction.cpp
  src/solver/LotSizingSolver.cpp
  src/solver/OUPolicySolver.cpp
)

# Search Module - Search Algorithms
set(SEARCH_SOURCES
  src/search/Mutations.cpp
  src/search/LocalSearch.cpp
)

# Core Module - Genetic Algorithm Core
set(CORE_SOURCES
  src/core/Genetic.cpp
  src/core/Population.cpp
  src/core/Individu.cpp
  src/search/Mutations.cpp
)

# RPC Module - Remote Procedure Call
set(RPC_SOURCES
  rpc/irp.pb.cc
  rpc/irp.grpc.pb.cc
)

# ============================================================================
# Module Library Build
# ============================================================================

# Utils Library
add_library(irp_utils STATIC ${UTILS_SOURCES})

# Model Library
add_library(irp_model STATIC ${MODEL_SOURCES})
target_link_libraries(irp_model PUBLIC irp_utils)

# Solver Library
add_library(irp_solver STATIC ${SOLVER_SOURCES})
target_link_libraries(irp_solver PUBLIC irp_model)

# Search Library (Depends on Model and Solver)
add_library(irp_search STATIC ${SEARCH_SOURCES})
target_link_libraries(irp_search PUBLIC irp_model irp_solver)

# Core Library (Depends on all other modules)
add_library(irp_core STATIC ${CORE_SOURCES})
target_link_libraries(irp_core PUBLIC irp_search irp_solver irp_model irp_utils)

# RPC Library
add_library(irp_rpc STATIC ${RPC_SOURCES})
target_link_libraries(irp_rpc PUBLIC ${GRPC_LDFLAGS})

# ============================================================================
# Main Program
# ============================================================================
add_executable(irp main.cpp)

target_link_libraries(irp PRIVATE
  irp_core
  irp_search
  irp_solver
  irp_model
  irp_utils
  irp_rpc
  m pthread dl
  ${GRPC_LDFLAGS}
)

# ============================================================================
# Installation Rules
# ============================================================================
install(TARGETS irp DESTINATION bin)
install(DIRECTORY include/irp DESTINATION include)

# ============================================================================
# Clean Targets
# ============================================================================
add_custom_target(clean_all
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/irp
  COMMENT "Completely clean build files"
)

# ============================================================================
# Build Information
# ============================================================================
message(STATUS "")
message(STATUS "============================================")
message(STATUS "IRP - Inventory Routing Problem Solver")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "============================================")
message(STATUS "Modules:")
message(STATUS " - irp_utils : Utilities")
message(STATUS " - irp_model : Data Models")
message(STATUS " - irp_solver : Inventory Solvers")
message(STATUS " - irp_search : Local Search")
message(STATUS " - irp_core  : Genetic Algorithm")
message(STATUS " - irp_rpc  : gRPC Support")
message(STATUS "============================================")
message(STATUS "")
