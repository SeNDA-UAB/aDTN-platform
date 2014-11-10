cmake_minimum_required (VERSION 2.8.2)

# Define installation prefix
SET(PREFIX "${CMAKE_INSTALL_PREFIX}")
add_definitions(-DPREFIX="${PREFIX}")

# Define configuration dir
if (NOT DEFINED SYSCONFDIR)
	set(SYSCONFDIR "${PREFIX}/etc/adtn" CACHE PATH "Path of the configuration files." FORCE)
endif()
add_definitions(-DSYSCONFDIR="${SYSCONFDIR}")

project(adtn C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include_directories(platform)
include_directories(lib)

add_subdirectory(platform)
add_subdirectory(lib)
add_subdirectory(examples)
