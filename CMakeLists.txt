cmake_minimum_required(VERSION 3.20)
project(goto10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(c64rnd c64rnd.cpp c64rnd.h)

add_executable(test_rnd test_rnd.cpp)
target_link_libraries(test_rnd PRIVATE c64rnd)
