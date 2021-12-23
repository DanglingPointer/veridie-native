project(VeriDie-core)

add_subdirectory(src)

if(veridie_build_tests)
    add_subdirectory(test)
endif()
