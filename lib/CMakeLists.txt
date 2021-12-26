cmake_minimum_required(VERSION 3.18.1)

project(VeriDie CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(veridie_build_tests "Build veridie unit tests." OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(coroutines)
include(runontarget)
include(warnings)

set_warnings(TREAT_AS_ERRORS)
enable_coroutines()

if(veridie_build_tests)
    enable_testing()
endif()

add_subdirectory(external)
add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(dispatch)
