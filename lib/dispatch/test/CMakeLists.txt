
add_executable(dispatchtests
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/worker.cpp
        test_threading.cpp
        )

target_link_libraries(dispatchtests
        PRIVATE
        GTest::gtest_main
        )

if(ANDROID)
    run_on_android(dispatchtests)
else()
    include(GoogleTest)
    gtest_discover_tests(dispatchtests)
endif()
