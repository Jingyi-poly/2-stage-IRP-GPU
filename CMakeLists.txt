cmake_minimum_required(VERSION 3.15)
project(HGS_CVRP LANGUAGES CXX CUDA)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)

find_package(CUDA  REQUIRED)
include_directories("${CUDA_INCLUDE_DIRS}")


find_package(OpenMP REQUIRED)

set(
        src_files
        Program/Genetic.cpp
        Program/Individual.cpp
        Program/LocalSearch.cpp
        Program/Params.cpp
        Program/Population.cpp
        Program/Split.cpp
        Program/InstanceCVRPLIB.cpp
        Program/AlgorithmParameters.cpp
        Program/C_Interface.cpp)

set(
        src_cuda_files
        Program/Genetic.cpp
        Program/Individual.cpp
        Program/LocalSearch.cpp
        Program/Params.cpp
        Program/Population.cpp
        Program/Split.cpp
        Program/SplitCUDA.cu
        Program/InstanceCVRPLIB.cpp
        Program/AlgorithmParameters.cpp
        Program/C_Interface.cpp)

if (MSVC)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif (MSVC)

include_directories(Program)

# object to be used by both static and shared (compile only once)
add_library(objlib OBJECT ${src_files})
add_library(objlib_cuda OBJECT ${src_cuda_files})

set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)
set_property(TARGET objlib_cuda PROPERTY POSITION_INDEPENDENT_CODE 1)
set_property(TARGET objlib_cuda PROPERTY CUDA_ARCHITECTURES 75)

# runtime library
add_library(lib SHARED $<TARGET_OBJECTS:objlib>)
add_library(lib_cuda SHARED $<TARGET_OBJECTS:objlib_cuda>)
set_target_properties(lib_cuda PROPERTIES OUTPUT_NAME hgscvrp CUDA_ARCHITECTURES 75)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # We're in the root, build everything
    # static library
    add_library(lib_static STATIC $<TARGET_OBJECTS:objlib>)
    add_library(lib_static_cuda STATIC $<TARGET_OBJECTS:objlib_cuda>)
    # if static and runtime libraries use name "hgscvrp", MSVC will overwrite one
    # of them, because both STATIC and SHARED builds create "hgscvrp.lib"
    set_target_properties(lib_static PROPERTIES OUTPUT_NAME hgscvrp_static)
    set_target_properties(lib_static_cuda PROPERTIES OUTPUT_NAME hgscvrp_static_cuda CUDA_ARCHITECTURES 75)

    # Build Executable

    add_executable(bin Program/main2.cpp)
    target_link_libraries(bin PRIVATE lib_static OpenMP::OpenMP_CXX)
    set_target_properties(bin PROPERTIES OUTPUT_NAME hgs)
    add_executable(bin_cuda Program/main_cuda.cpp)
    target_link_libraries(bin_cuda PRIVATE lib_static_cuda OpenMP::OpenMP_CXX)
    set_target_properties(bin_cuda PROPERTIES OUTPUT_NAME hgs_cuda CUDA_ARCHITECTURES 75)


    # Test Executable

    include(CTest)
    add_test(NAME    bin_test_X-n101-k25
             COMMAND ${CMAKE_COMMAND}   -DINSTANCE=X-n101-k25
                                        -DCOST=27591
                                        -DROUND=1
                                        -P ${PROJECT_SOURCE_DIR}/Test/TestExecutable.cmake)
    add_test(NAME    bin_test_X-n106-k14
             COMMAND ${CMAKE_COMMAND}   -DINSTANCE=X-n110-k13
                                        -DCOST=14971
                                        -DROUND=1
                                        -P ${PROJECT_SOURCE_DIR}/Test/TestExecutable.cmake)

    # Test Executable: Instances with Duration, without Rounding
    add_test(NAME    bin_test_CMT6
            COMMAND ${CMAKE_COMMAND}    -DINSTANCE=CMT6
                                        -DCOST=555.43
                                        -DROUND=0
                                        -P ${PROJECT_SOURCE_DIR}/Test/TestExecutable.cmake)
    add_test(NAME    bin_test_CMT7
            COMMAND ${CMAKE_COMMAND}    -DINSTANCE=CMT7
                                        -DCOST=909.675
                                        -DROUND=0
                                        -P ${PROJECT_SOURCE_DIR}/Test/TestExecutable.cmake)

    # Test Library
    add_subdirectory(Test/Test-c/)
    add_test(NAME       lib_test_c
             COMMAND    lib_test_c)

    # Install
    install(TARGETS lib
            DESTINATION lib)
    install(TARGETS bin
            DESTINATION bin)
    install(TARGETS lib_cuda
            DESTINATION lib_cuda)
    install(TARGETS bin_cuda
            DESTINATION bin_cuda)
    install(FILES Program/AlgorithmParameters.h Program/C_Interface.h
            DESTINATION include)
endif()

