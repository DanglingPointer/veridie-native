
add_executable(jnitests
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/worker.cpp
        test_threading.cpp
        )

target_link_libraries(jnitests
        PRIVATE
        GTest::gtest_main
        )

if(ANDROID)
    run_on_android(jnitests)
else()
    include(GoogleTest)
    gtest_discover_tests(jnitests)
endif()
