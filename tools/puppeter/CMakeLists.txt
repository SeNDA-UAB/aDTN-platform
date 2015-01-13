cmake_minimum_required (VERSION 2.8.2)
project(PUPPETER CXX)

add_executable(puppeter puppeter.cpp)
target_link_libraries(puppeter dyninstAPI)
target_link_libraries(puppeter pthread)
target_link_libraries(puppeter rt)
target_link_libraries(puppeter instructionAPI)