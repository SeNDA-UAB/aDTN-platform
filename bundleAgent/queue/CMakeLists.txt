cmake_minimum_required (VERSION 2.8.2) 

#queue 
include_directories(include ${CMAKE_SOURCE_DIR})

add_executable(queueManager queueManager.c)
target_link_libraries(queueManager adtn)

include_directories(../common/include ../common/  ${CMAKE_SOURCE_DIR})

install(TARGETS queueManager DESTINATION bin)
