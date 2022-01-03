
add_library(veridie-utils
        STATIC
        format.cpp include/utils/format.hpp
        log.cpp include/utils/log.hpp
        include/utils/alwayscopyable.hpp
        include/utils/coroutine.hpp
        include/utils/mempool.hpp
        include/utils/poolbuilder.hpp
        include/utils/poolptr.hpp
        include/utils/task.hpp
        include/utils/taskowner.hpp
        include/utils/taskutils.hpp
        )

target_compile_features(veridie-utils
        PUBLIC cxx_std_20
        )

target_include_directories(veridie-utils
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        )

add_library(veridie::utils ALIAS veridie-utils)
