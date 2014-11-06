cmake_minimum_required (VERSION 2.8.2)

project(adtn C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include_directories(platform)
include_directories(lib)

add_subdirectory(platform)
add_subdirectory(lib)
add_subdirectory(examples)
