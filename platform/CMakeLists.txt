cmake_minimum_required (VERSION 2.8.2)

# Define installation prefix
SET(PREFIX "${CMAKE_INSTALL_PREFIX}")
add_definitions(-DPREFIX="${PREFIX}")

# Define configuration dir
if (NOT DEFINED SYSCONFDIR)
	set(SYSCONFDIR "${PREFIX}/etc" CACHE PATH "Path of the configuration files." FORCE)
endif()
add_definitions(-DSYSCONFDIR="${SYSCONFDIR}")

project(PLATFORM C CXX)

# If binaries should have debug information 
STRING(TOLOWER "${BTYPE}" BTYPE)
if(BTYPE STREQUAL "debug")
	MESSAGE("-- [INFO] Compiling platform as debug")
	SET(CMAKE_BUILD_TYPE DEBUG)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG} -Wall -ggdb")
else()
	MESSAGE("-- [INFO] Compiling platform as release")
	SET(CMAKE_BUILD_TYPE RELEASE)
	SET(CMAKE_C_FLAGS "-O3")
endif()

# If tests should be build
STRING(TOLOWER "${PROF}" PROF)
if(PROF STREQUAL "true")
	MESSAGE("-- [INFO] Profiling enabled (i.e. test will be compiled)")
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
else()
	MESSAGE("-- [INFO] Profiling disabled (i.e. test won't be compiled)")
endif()
 
# Deprecated (the presence of logs will be decided at run time)
# If the paltform should produce log files
STRING(TOLOWER "${NOLOG}" NOLOG)
if(NOLOG STREQUAL "true")
	add_definitions(-DNOLOG=1)
	MESSAGE("-- [INFO] Compiling without logging into files")
endif()

add_subdirectory(common)
add_subdirectory(information_exchange)
add_subdirectory(core)
add_subdirectory(queue)
add_subdirectory(executor)

# Configure adtnd script: Installation prefix and default configuration file path.
file(READ "${PLATFORM_SOURCE_DIR}/adtnd/adtnd.sh" FILE_CONTENT)
string(REGEX REPLACE "INSTALL_PREFIX=[^ \n]*" "INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
string(REGEX REPLACE "config_file=\\$INSTALL_PREFIX[^ \n]*" "config_file=${SYSCONFDIR}/adtn/adtn.ini" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/adtnd/adtnd" "${MODIFIED_FILE_CONTENT}")

# Configure adtn.ini: change platofrm id to computer's hostname and set where are the default codes located.
EXECUTE_PROCESS(COMMAND hostname OUTPUT_VARIABLE HOSTNAME)
STRING(REGEX REPLACE "(\r?\n)+$" "" hostname "${HOSTNAME}")
file(READ "${PLATFORM_SOURCE_DIR}/config/adtn.ini" FILE_CONTENT)
string(REGEX REPLACE "id = [^\n]*" "id = ${HOSTNAME}" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
string(REGEX REPLACE "default_codes" "${SYSCONFDIR}/adtn/default_codes" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/config/adtn.ini" "${MODIFIED_FILE_CONTENT}")


# Installation

# Needed to create the .deb (all files should be in the binay dir)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/life.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/prio.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/routing.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)

# Configuration files
install(FILES ${PLATFORM_BINARY_DIR}/config/adtn.ini DESTINATION ${SYSCONFDIR})
install(FILES ${PLATFORM_BINARY_DIR}/adtnd/adtnd PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ DESTINATION bin)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/life.c DESTINATION ${SYSCONFDIR}/adtn/default_codes)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/prio.c DESTINATION ${SYSCONFDIR}/adtn/default_codes)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/routing.c DESTINATION ${SYSCONFDIR}/adtn/default_codes)
