
add_executable(coretests
        fakelogger.hpp
        test_dice.cpp
        test_cmd.cpp
        test_controller.cpp
        test_timer.cpp
        )
target_include_directories(coretests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src) # hack in order to test internal classes

target_link_libraries(coretests
        PRIVATE
        GTest::gtest_main
        veridie::core
        )

if(ANDROID)
    run_on_android(coretests)
else()
    include(GoogleTest)
    gtest_discover_tests(coretests)
endif()
