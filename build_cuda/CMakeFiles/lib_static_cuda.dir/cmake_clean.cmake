file(REMOVE_RECURSE
  "libhgscvrp_static_cuda.a"
  "libhgscvrp_static_cuda.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CUDA CXX)
  include(CMakeFiles/lib_static_cuda.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
