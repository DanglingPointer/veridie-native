include(FetchContent)

if(veridie_build_tests)

    # Fetch googletest. Hopefully this will bring in gtest_main
    FetchContent_Declare(googletest
      GIT_REPOSITORY    https://github.com/google/googletest.git
      GIT_TAG           release-1.11.0
      SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googletest-src"
      BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googletest-build"
    )
    # Don't build gmock
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

endif()
