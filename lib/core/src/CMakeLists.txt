
function(add_core_library name)
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


add_core_library(core
        bt/device.hpp
        ctrl/controller.cpp include/ctrl/controller.hpp
        ctrl/commandadapter.cpp ctrl/commandadapter.hpp
        ctrl/timer.cpp include/ctrl/timer.hpp
        dice/engine.cpp include/dice/engine.hpp
        dice/serializer.cpp include/dice/serializer.hpp
        dice/xmlparser.hpp
        include/dice/cast.hpp
        include/sign/cmd.hpp
        include/sign/externalinvoker.hpp
        fsm/context.hpp
        fsm/statebase.hpp
        fsm/stateconnecting.cpp fsm/stateconnecting.hpp
        fsm/stateidle.cpp fsm/stateidle.hpp
        fsm/statenegotiating.cpp fsm/statenegotiating.hpp
        fsm/stateplaying.cpp fsm/stateplaying.hpp
        sign/commandmanager.cpp sign/commandmanager.hpp
        sign/commands.cpp sign/commands.hpp
        sign/commandpool.hpp
        sign/events.cpp sign/events.hpp
        )

add_core_library(fakecore
        ctrl/echocontroller.cpp include/ctrl/controller.hpp
        sign/commandmanager.cpp sign/commandmanager.hpp
        sign/commandpool.hpp
        sign/commands.hpp
        include/ctrl/controller.hpp
        include/dice/cast.hpp
        include/dice/engine.hpp
        include/dice/serializer.hpp
        include/sign/cmd.hpp
        include/sign/externalinvoker.hpp
        )
