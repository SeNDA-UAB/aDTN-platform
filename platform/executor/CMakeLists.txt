cmake_minimum_required (VERSION 2.8.2)

PROJECT(EXECUTOR C)

find_package( Threads )

#Compile helpers
add_subdirectory(modules/exec_c_helpers)

STRING(TOLOWER "${PROF}" PROF)
if(PROF STREQUAL "true")
	MESSAGE("-- [INFO] Including test from ${CMAKE_CURRENT_SOURCE_DIR}/test")
	add_subdirectory(test)
else()
	MESSAGE("-- [INFO] Not including test from ${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()

include_directories(${PLATFORM_SOURCE_DIR} ${EXECUTOR_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
#include_directories(../ ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/modules/exec_c_helpers/include)
add_executable(executor modules/hash.c modules/exec.c modules/codes.c executor.c)

target_link_libraries(executor ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(executor dl)
target_link_libraries(executor adtn)

install(TARGETS executor DESTINATION bin)
