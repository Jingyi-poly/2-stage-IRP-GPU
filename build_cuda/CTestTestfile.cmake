# CMake generated Testfile for 
# Source directory: /mnt/sda2/git/HGS-CVRP
# Build directory: /mnt/sda2/git/HGS-CVRP/build_cuda
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(bin_test_X-n101-k25 "/usr/bin/cmake" "-DINSTANCE=X-n101-k25" "-DCOST=27591" "-DROUND=1" "-P" "/mnt/sda2/git/HGS-CVRP/Test/TestExecutable.cmake")
set_tests_properties(bin_test_X-n101-k25 PROPERTIES  _BACKTRACE_TRIPLES "/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;79;add_test;/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;0;")
add_test(bin_test_X-n106-k14 "/usr/bin/cmake" "-DINSTANCE=X-n110-k13" "-DCOST=14971" "-DROUND=1" "-P" "/mnt/sda2/git/HGS-CVRP/Test/TestExecutable.cmake")
set_tests_properties(bin_test_X-n106-k14 PROPERTIES  _BACKTRACE_TRIPLES "/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;84;add_test;/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;0;")
add_test(bin_test_CMT6 "/usr/bin/cmake" "-DINSTANCE=CMT6" "-DCOST=555.43" "-DROUND=0" "-P" "/mnt/sda2/git/HGS-CVRP/Test/TestExecutable.cmake")
set_tests_properties(bin_test_CMT6 PROPERTIES  _BACKTRACE_TRIPLES "/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;91;add_test;/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;0;")
add_test(bin_test_CMT7 "/usr/bin/cmake" "-DINSTANCE=CMT7" "-DCOST=909.675" "-DROUND=0" "-P" "/mnt/sda2/git/HGS-CVRP/Test/TestExecutable.cmake")
set_tests_properties(bin_test_CMT7 PROPERTIES  _BACKTRACE_TRIPLES "/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;96;add_test;/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;0;")
add_test(lib_test_c "/mnt/sda2/git/HGS-CVRP/build_cuda/Test/Test-c/lib_test_c")
set_tests_properties(lib_test_c PROPERTIES  _BACKTRACE_TRIPLES "/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;104;add_test;/mnt/sda2/git/HGS-CVRP/CMakeLists.txt;0;")
subdirs("Test/Test-c")
