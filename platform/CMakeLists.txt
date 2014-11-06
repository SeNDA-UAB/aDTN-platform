cmake_minimum_required (VERSION 2.8.2)

project(PLATFORM C CXX)

SET(PREFIX "\"${CMAKE_INSTALL_PREFIX}\"")
add_definitions(-DPREFIX=${PREFIX})

STRING(TOLOWER "${BTYPE}" BTYPE)
if(BTYPE STREQUAL "debug")
	MESSAGE("-- [INFO] Compiling platform as debug")
	SET(CMAKE_BUILD_TYPE DEBUG)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG} -Wall -ggdb") #-p flag
else()
	MESSAGE("-- [INFO] Compiling platform as release")
	SET(CMAKE_BUILD_TYPE RELEASE)
	SET(CMAKE_C_FLAGS "-O3")
endif()

STRING(TOLOWER "${PROF}" PROF)
if(PROF STREQUAL "true")
	MESSAGE("-- [INFO] Profiling enabled (i.e: test will be compiled)")
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
	#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wmissing-braces")
else()
	MESSAGE("-- [INFO] Profiling disabled (i.e: test won't be compiled)")
endif()

STRING(TOLOWER "${NOLOG}" NOLOG)
if(NOLOG STREQUAL "true")
	add_definitions(-DNOLOG=1)
	MESSAGE("-- [INFO] Compiling without logging into files")
endif()

file(MAKE_DIRECTORY common)
add_subdirectory(common)
add_subdirectory(information_exchange)
add_subdirectory(core)
add_subdirectory(queue)

#file(MAKE_DIRECTORY executor)
add_subdirectory(executor)

if(PREFIX STREQUAL "\"/usr\"")
SET(INIPATH "/etc/adtn/")
else()
SET(INIPATH "${CMAKE_INSTALL_PREFIX}/etc/adtn/")
endif()

file(READ "${PLATFORM_SOURCE_DIR}/adtnd/adtnd.sh" FILE_CONTENT)
string(REGEX REPLACE "INSTALL_PREFIX=[^ \n]*" "INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/adtnd/adtnd" "${MODIFIED_FILE_CONTENT}")

file(READ "${PLATFORM_BINARY_DIR}/adtnd/adtnd" FILE_CONTENT)
string(REGEX REPLACE "config_file=\\$INSTALL_PREFIX[^ \n]*" "config_file=${INIPATH}adtn.ini" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/adtnd/adtnd" "${MODIFIED_FILE_CONTENT}")

EXECUTE_PROCESS(COMMAND hostname OUTPUT_VARIABLE hostname)
STRING(REGEX REPLACE "(\r?\n)+$" "" hostname "${hostname}")
file(READ "${PLATFORM_SOURCE_DIR}/config/adtn.ini" FILE_CONTENT)
string(REGEX REPLACE "id = [^\n]*" "id = ${hostname}" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/config/adtn.ini" "${MODIFIED_FILE_CONTENT}")

file(READ "${PLATFORM_BINARY_DIR}/config/adtn.ini" FILE_CONTENT)
string(REGEX REPLACE "default_codes" "${INIPATH}default_codes" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/config/adtn.ini" "${MODIFIED_FILE_CONTENT}")

file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/life.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/prio.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/routing.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)

install(FILES ${PLATFORM_BINARY_DIR}/adtnd/adtnd PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ DESTINATION bin)
install(FILES ${PLATFORM_BINARY_DIR}/config/adtn.ini DESTINATION /etc/adtn)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/life.c DESTINATION /etc/adtn/default_codes)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/prio.c DESTINATION /etc/adtn/default_codes)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/routing.c DESTINATION /etc/adtn/default_codes)
