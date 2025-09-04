file(REMOVE_RECURSE
  "libhgscvrp_static.a"
  "libhgscvrp_static.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lib_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
