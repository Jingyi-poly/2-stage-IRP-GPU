file(REMOVE_RECURSE
  "libhgscvrp.pdb"
  "libhgscvrp.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang CUDA CXX)
  include(CMakeFiles/lib_cuda.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
