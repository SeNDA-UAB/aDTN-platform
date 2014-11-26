cmake_minimum_required (VERSION 2.8.2)

project(EXECUTOR C)

find_package(Threads)

#Compile helpers
add_subdirectory(modules/exec_c_helpers)

include_directories(${PLATFORM_SOURCE_DIR} ${EXECUTOR_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
add_executable(executor modules/hash.c modules/exec.c modules/codes.c executor.c)

target_link_libraries(executor ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(executor dl)
target_link_libraries(executor adtn)

install(TARGETS executor DESTINATION bin)
