cmake_minimum_required(VERSION 3.0.2)
project(nlib_tests)

add_compile_options(-std=c++17)
add_compile_options(-O3)



add_executable (test_time_hysteresis test_time_hysteresis.cpp)
target_link_libraries (test_time_hysteresis dl)
