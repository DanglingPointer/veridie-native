
function(veridie_library name)
    set(library_name "veridie-${name}")
    add_library(${library_name} STATIC ${ARGN})
    target_compile_features(${library_name} PUBLIC cxx_std_20)
    target_include_directories(${library_name}
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${library_name}
            PUBLIC veridie::utils)
    add_library(veridie::${name} ALIAS ${library_name})
endfunction()


veridie_library(core
        ctrl/controller.cpp
        ctrl/commandadapter.cpp
        ctrl/timer.cpp
        dice/engine.cpp
        dice/serializer.cpp
        fsm/stateconnecting.cpp
        fsm/stateidle.cpp
        fsm/statenegotiating.cpp
        fsm/stateplaying.cpp
        sign/commandmanager.cpp
        sign/commands.cpp
        sign/events.cpp
        )

veridie_library(fakecore
        ctrl/echocontroller.cpp
        sign/commandmanager.cpp
        )
