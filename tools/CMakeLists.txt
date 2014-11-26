cmake_minimum_required (VERSION 2.8.2)
project(EXAMPLES C)

message("-- Starting tools build configuration")

include(../aDTNConfig.cmake.in)

find_package( Threads )

include_directories(${PROJECT_SOURCE_DIR}/..)
add_executable(adtn-ping adtn-ping.c)
add_executable(adtn-neighbours adtn-neighbours.c)
add_executable(adtn-traceroute adtn-traceroute.c)

target_link_libraries(adtn-ping pthread)
target_link_libraries(adtn-ping adtn)
target_link_libraries(adtn-ping adtnAPI)
target_link_libraries(adtn-neighbours adtn)
target_link_libraries(adtn-traceroute pthread)
target_link_libraries(adtn-traceroute adtn)
target_link_libraries(adtn-traceroute adtnAPI)

install(TARGETS adtn-ping DESTINATION bin)
install(TARGETS adtn-neighbours DESTINATION bin)
install(TARGETS adtn-traceroute DESTINATION bin)
