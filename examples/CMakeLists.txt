cmake_minimum_required (VERSION 2.8.2)

PROJECT(EXECUTOR C)

find_package( Threads )

STRING(TOLOWER "${BTYPE}" BTYPE)
if(BTYPE STREQUAL "debug")
	MESSAGE("-- [INFO] Compiling lib as debug") 
	SET(CMAKE_BUILD_TYPE DEBUG)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG} -Wall -ggdb") #-p flag
else()
	MESSAGE("-- [INFO] Compiling lib as release") 
	SET(CMAKE_BUILD_TYPE RELEASE)
	SET(CMAKE_C_FLAGS "-O3")
endif()

include_directories(${PROJECT_SOURCE_DIR}/../platform ${PROJECT_SOURCE_DIR}/../lib)
add_executable(adtn-ping adtn-ping.c)

target_link_libraries(adtn-ping pthread)
target_link_libraries(adtn-ping adtn)
target_link_libraries(adtn-ping adtnAPI)

install(TARGETS adtn-ping DESTINATION bin)
