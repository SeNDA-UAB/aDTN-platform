cmake_minimum_required (VERSION 2.8.2)
project(PUPPETER CXX)
list( APPEND CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS} -g -ftest-coverage -fprofile-arcs")
#cmake_policy(SET CMP0037 NEW)

include_directories(include /home/xeri/projects/adtn/root/include)

add_library(puppetLib SHARED puppetLib.c)
set_property(SOURCE puppetLib.c APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-unused-function -Wno-unused-variable")

add_library(puppetMasterLib SHARED puppetMasterLib.cpp puppetStats.cpp)

add_subdirectory(tests)