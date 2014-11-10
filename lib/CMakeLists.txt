cmake_minimum_required (VERSION 2.8.2)

MESSAGE("-- [INFO] Starting cmake of lib")

# Define installation prefix
SET(PREFIX "${CMAKE_INSTALL_PREFIX}")
add_definitions(-DPREFIX="${PREFIX}")

# Define configuration dir
if (NOT DEFINED SYSCONFDIR)
	set(SYSCONFDIR "${PREFIX}/etc/adtn" CACHE PATH "Path of the configuration files." FORCE)
endif()
add_definitions(-DSYSCONFDIR="${SYSCONFDIR}")

project(lib C CXX)

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

include_directories(${PROJECT_SOURCE_DIR}/../platform)

file(MAKE_DIRECTORY api)
add_subdirectory(api)

install(FILES ${PROJECT_SOURCE_DIR}/api/include/adtn.h DESTINATION include)
install(FILES ${PROJECT_BINARY_DIR}/api/libadtnAPI.so PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE DESTINATION lib)
