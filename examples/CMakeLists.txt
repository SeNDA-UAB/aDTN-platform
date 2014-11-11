cmake_minimum_required (VERSION 2.8.2)
project(EXAMPLES C)

message("-- Starting examples build configuration")

include(../aDTNConfig.cmake.in)

find_package( Threads )

include_directories(${PROJECT_SOURCE_DIR}/../platform ${PROJECT_SOURCE_DIR}/../lib)
add_executable(adtn-ping adtn-ping.c)

target_link_libraries(adtn-ping pthread)
target_link_libraries(adtn-ping adtn)
target_link_libraries(adtn-ping adtnAPI)

install(TARGETS adtn-ping DESTINATION bin)
