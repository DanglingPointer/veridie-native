function(run_on_android target_name)

    if (NOT ANDROID)
        message(FATAL_ERROR "run_on_android: Not Android")
    endif()

    if (NOT TARGET ${target_name})
        message(FATAL_ERROR "run_on_android: Could not find target ${target_name}")
    endif()

    find_program(ADB ${ANDROID_NDK}/../../platform-tools/adb)
    if(NOT ADB)
        message(FATAL_ERROR "run_on_android: Could not find adb")
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${ADB} shell mkdir -p /data/local/tmp/${ANDROID_ABI}
            COMMAND ${ADB} push $<TARGET_FILE:${target_name}> /data/local/tmp/${ANDROID_ABI}/
            COMMAND ${ADB} shell \"export LD_LIBRARY_PATH=/data/local/tmp/${ANDROID_ABI}\; /data/local/tmp/${ANDROID_ABI}/${target_name}\")

endfunction()
