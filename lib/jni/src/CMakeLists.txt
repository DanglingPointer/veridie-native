
find_library(log-lib
        log)

macro(add_jni_library name core_flavor)
    add_library(${name}
            SHARED
            javainvoker.cpp javainvoker.hpp
            jniexec.cpp jniexec.hpp
            mainexec.cpp mainexec.hpp
            worker.cpp worker.hpp
            )
    target_compile_features(${name}
            PUBLIC cxx_std_20
            )
    target_include_directories(${name}
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            )
    target_link_libraries(${name}
            PUBLIC
            ${log-lib}
            veridie::utils
            veridie::${core_flavor}
            )
    add_library(veridie::${name} ALIAS ${name})
endmacro()

add_jni_library(veridie
        core)

add_jni_library(jni_only_veridie
        fakecore)
