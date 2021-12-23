
add_executable(utilstests
        test_log.cpp
        test_mempool.cpp
        test_task.cpp
        )

target_link_libraries(utilstests
        PRIVATE
        GTest::gtest_main
        veridie::utils
        )

if(ANDROID)
    run_on_android(utilstests)
else()
    include(GoogleTest)
    gtest_discover_tests(utilstests)
endif()
