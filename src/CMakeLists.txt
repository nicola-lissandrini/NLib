cmake_minimum_required(VERSION 3.0.2)
project(test)

add_compile_options(-std=c++17)
add_compile_options(-O2)

add_executable (test_timeseries test_timeseries.cpp)
