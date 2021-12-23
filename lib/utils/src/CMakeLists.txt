
add_library(veridie-utils
        STATIC
        format.cpp
        log.cpp
        )

target_compile_features(veridie-utils
        PUBLIC cxx_std_20
        )

target_include_directories(veridie-utils
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        )

add_library(veridie::utils ALIAS veridie-utils)
