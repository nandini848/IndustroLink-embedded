cmake_minimum_required(VERSION 3.16)

project(IndustroLink LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(industrolink_app main.cpp)

target_link_libraries(industrolink_app
    PRIVATE Threads::Threads
)

enable_testing()

find_package(GTest REQUIRED)

add_executable(industrolink_tests
    tests/test_industrolink.cpp
)

target_link_libraries(industrolink_tests
    PRIVATE
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(industrolink_tests)
