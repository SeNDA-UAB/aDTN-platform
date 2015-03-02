cmake_minimum_required (VERSION 2.8.2)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-write-strings -Wno-literal-suffix -pg")

include_directories(../)

add_executable(bundleAgent_Test_1 bundleAgent_Test_1.cpp)
target_link_libraries(bundleAgent_Test_1 adtn)
target_link_libraries(bundleAgent_Test_1 puppetMasterLib)
target_link_libraries(bundleAgent_Test_1 dyninstAPI)
target_link_libraries(bundleAgent_Test_1 dyninstAPI_RT)