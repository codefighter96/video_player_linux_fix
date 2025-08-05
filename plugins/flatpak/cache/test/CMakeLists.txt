set(TESTCASE_NAME "flatpak_plugin_test_cache_manager")

set(CMAKE_THREAD_PREFER_PTHREAD ON)
include(FindThreads)
add_executable(${TESTCASE_NAME}
        test_cache_manager.cc
)

target_compile_options(gtest PRIVATE -Wno-error=sign-conversion)
target_link_libraries(${TESTCASE_NAME} PRIVATE
        plugin_common
        plugin_flatpak_cache
        gtest
        gtest_main
        ${CMAKE_THREAD_LIBS_INIT}
)

add_test(
        NAME ${TESTCASE_NAME}
        COMMAND ${TESTCASE_NAME}
)