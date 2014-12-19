cmake_minimum_required (VERSION 2.8.2)
project(ADTN C CXX)

message("-- Starting aDTN build configuration")

include(aDTNConfig.cmake.in)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include_directories(bundleAgent)
include_directories(lib)

add_subdirectory(bundleAgent)
add_subdirectory(lib)
add_subdirectory(tools)
