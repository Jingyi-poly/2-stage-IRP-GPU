cmake_minimum_required(VERSION 3.15)
project(cvrp_exact LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP REQUIRED)

set(GUROBI_HOME "/opt/gurobi1201/linux64")

set(HGS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Program")

set(hgs_sources
    ${HGS_SRC_DIR}/Params.cpp
    ${HGS_SRC_DIR}/Individual.cpp
    ${HGS_SRC_DIR}/Split.cpp
    ${HGS_SRC_DIR}/InstanceCVRPLIB.cpp
    ${HGS_SRC_DIR}/AlgorithmParameters.cpp
    ${HGS_SRC_DIR}/LocalSearch.cpp
    ${HGS_SRC_DIR}/Population.cpp
    ${HGS_SRC_DIR}/Genetic.cpp
    ${HGS_SRC_DIR}/GeneticHGS.cpp
    ${HGS_SRC_DIR}/C_Interface.cpp)

add_executable(cvrp_exact cvrp_exact.cpp ${hgs_sources})
target_include_directories(cvrp_exact PRIVATE ${HGS_SRC_DIR} ${GUROBI_HOME}/include)
target_link_libraries(cvrp_exact PRIVATE OpenMP::OpenMP_CXX
    ${GUROBI_HOME}/lib/libgurobi_c++.a ${GUROBI_HOME}/lib/libgurobi120.so)
