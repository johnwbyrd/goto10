cmake_minimum_required(VERSION 3.20)
project(goto10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(msbasicrnd
    src/msbasicrnd.h
    src/msbasicfprnd.h
    src/msbasicfprnd.cpp
    src/c64rnd.h
    src/c64rnd.cpp
    src/applesoftrnd.h
    src/applesoftrnd.cpp
)
target_include_directories(msbasicrnd PUBLIC src)

add_executable(test_rnd src/test_rnd.cpp)
target_link_libraries(test_rnd PRIVATE msbasicrnd)

add_executable(cycle_detect src/cycle_detect.cpp)
target_link_libraries(cycle_detect PRIVATE msbasicrnd)

add_executable(best_seed src/best_seed.cpp)
target_link_libraries(best_seed PRIVATE msbasicrnd)

add_executable(gen_seeds src/gen_seeds.cpp)
target_link_libraries(gen_seeds PRIVATE msbasicrnd)
