
if(ANDROID)
    # Android log library
    find_library(log-lib
            log)
endif()

macro(add_main_library name core_flavor)
    add_library(${name}
            SHARED
            common/mainexec.cpp common/mainexec.hpp
            common/worker.cpp common/worker.hpp
            )

    if(ANDROID)
        # Adding jni-dependent sources only when compiling for Android
        target_sources(${name}
                PRIVATE
                android/javainvoker.cpp android/javainvoker.hpp
                android/jniexec.cpp android/jniexec.hpp
                )
        target_link_libraries(${name}
                PUBLIC
                ${log-lib}
                )
    endif()

    target_compile_features(${name}
            PUBLIC cxx_std_20
            )
    target_include_directories(${name}
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            )
    target_link_libraries(${name}
            PUBLIC
            veridie::utils
            veridie::${core_flavor}
            )
    add_library(veridie::${name} ALIAS ${name})
endmacro()

add_main_library(veridie
        core)

add_main_library(jni_only_veridie
        fakecore)
