cmake_minimum_required(VERSION 3.10)
project(CurlClientTest CXX)

set(TESTCASE_NAME "CurlClientTest")
set(TESTCASE_CC test_curl_client.cc)
set(CURL_CLIENT_CC ../curl_client.cc)

add_executable(
    ${TESTCASE_NAME}
    ${TESTCASE_CC}
    ${CURL_CLIENT_CC}
)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)

target_compile_features(${TESTCASE_NAME} PRIVATE cxx_std_17)

target_include_directories(
    ${TESTCASE_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(
    ${TESTCASE_NAME}
    PRIVATE
    CURL::libcurl
    spdlog::spdlog
    fmt::fmt
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
)

add_test(
    NAME ${TESTCASE_NAME}
    COMMAND ${TESTCASE_NAME}
)
