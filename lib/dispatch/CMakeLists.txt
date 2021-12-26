project(VeriDie-dispatch CXX)

add_subdirectory(src)

if(veridie_build_tests)
    add_subdirectory(test)
endif()
