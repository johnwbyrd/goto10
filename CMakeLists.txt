cmake_minimum_required(VERSION 3.20)
project(goto10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(c64rnd src/c64rnd.cpp src/c64rnd.h)
target_include_directories(c64rnd PUBLIC src)

add_executable(test_rnd src/test_rnd.cpp)
target_link_libraries(test_rnd PRIVATE c64rnd)

add_executable(cycle_detect src/cycle_detect.cpp)
target_link_libraries(cycle_detect PRIVATE c64rnd)

add_executable(compare_seeds src/compare_seeds.cpp)
target_link_libraries(compare_seeds PRIVATE c64rnd)

add_executable(compare_sampled src/compare_sampled.cpp)
target_link_libraries(compare_sampled PRIVATE c64rnd)

add_executable(best_seed src/best_seed.cpp)
target_link_libraries(best_seed PRIVATE c64rnd)

add_executable(debug_multiply src/debug_multiply.cpp)
target_link_libraries(debug_multiply PRIVATE c64rnd)

add_executable(cycle_zeros src/cycle_zeros.cpp)
target_link_libraries(cycle_zeros PRIVATE c64rnd)

add_executable(gen_seeds src/gen_seeds.cpp)
target_link_libraries(gen_seeds PRIVATE c64rnd)
