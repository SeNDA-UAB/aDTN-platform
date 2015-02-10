cmake_minimum_required (VERSION 2.8.2)
project(PUPPETER CXX)
list( APPEND CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS} -g -ftest-coverage -fprofile-arcs")
#cmake_policy(SET CMP0037 NEW)

include_directories(include /home/xeri/projects/adtn/root/include)

add_library(puppetLib SHARED puppetLib.c)
set_property(SOURCE puppetLib.c APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-unused-function -Wno-unused-variable")

add_library(puppetMasterLib SHARED puppetMasterLib.cpp)

add_executable(bundleAgentTest bundleAgentTest.cpp)
target_link_libraries(bundleAgentTest adtn)
target_link_libraries(bundleAgentTest puppetMasterLib)
target_link_libraries(bundleAgentTest dyninstAPI)
target_link_libraries(bundleAgentTest pthread)

add_executable(executorTest executorTest.cpp)
target_link_libraries(executorTest adtn)
target_link_libraries(executorTest puppetMasterLib)
target_link_libraries(executorTest dyninstAPI)
target_link_libraries(executorTest pthread)

add_executable(sendBundles sendBundles.c)
target_link_libraries(sendBundles adtn)
