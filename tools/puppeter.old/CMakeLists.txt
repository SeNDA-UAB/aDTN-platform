cmake_minimum_required (VERSION 2.8.2)
project(PUPPETER CXX)

include_directories(include /home/xeri/projects/adtn/root/include)

add_executable(puppeter puppeter.cpp)
target_link_libraries(puppeter dyninstAPI)
target_link_libraries(puppeter pthread)
target_link_libraries(puppeter rt)
target_link_libraries(puppeter instructionAPI)
target_link_libraries(puppeter adtn)

add_executable(ct_puppeter ct_puppeter.cpp)
target_link_libraries(ct_puppeter dyninstAPI)
target_link_libraries(ct_puppeter pthread)
target_link_libraries(ct_puppeter rt)
target_link_libraries(ct_puppeter instructionAPI)
target_link_libraries(ct_puppeter adtn)

add_executable(executeCode executeCode.c)
target_link_libraries(executeCode adtn)