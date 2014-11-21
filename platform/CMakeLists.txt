cmake_minimum_required (VERSION 2.8.2)
project(PLATFORM C CXX)

message("-- Starting platform build configuration")

include(../aDTNConfig.cmake.in)

add_subdirectory(common)
add_subdirectory(information_exchange)
add_subdirectory(core)
add_subdirectory(queue)
add_subdirectory(executor)

# Configure adtnd script: Installation prefix and default configuration file path.
file(READ "${PLATFORM_SOURCE_DIR}/adtnd/adtnd.sh" FILE_CONTENT)
string(REGEX REPLACE "INSTALL_PREFIX=[^ \n]*" "INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
string(REGEX REPLACE "config_file=\\$INSTALL_PREFIX[^ \n]*" "config_file=$INSTALL_PREFIX/adtn.ini" MODIFIED_FILE_CONTENT "${MODIFIED_FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/adtnd/adtnd" "${MODIFIED_FILE_CONTENT}")

# Configure adtn.ini: change platform id to computer's hostname, log granularity, and set where are the default codes located.
string(TOLOWER ${CMAKE_BUILD_TYPE} BTYPE)
execute_process(COMMAND hostname OUTPUT_VARIABLE HOSTNAME)
string(REGEX REPLACE "(\r?\n)+$" "" hostname "${HOSTNAME}")
file(READ "${PLATFORM_SOURCE_DIR}/config/adtn.ini" FILE_CONTENT)
string(REGEX REPLACE "id = [^\n]*" "id = ${HOSTNAME}" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
if (${BTYPE} STREQUAL "release")
	string(REGEX REPLACE "log_level = [^\n]*" "log_level = 1" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
else()
	string(REGEX REPLACE "log_level = [^\n]*" "log_level = 4" MODIFIED_FILE_CONTENT "${FILE_CONTENT}")
endif()
string(REGEX REPLACE "default_codes" "${SYSCONFDIR}/${ADTN_CONF_SUBDIR}/default_codes" MODIFIED_FILE_CONTENT "${MODIFIED_FILE_CONTENT}")
file(WRITE "${PLATFORM_BINARY_DIR}/config/adtn.ini" "${MODIFIED_FILE_CONTENT}")

# Installation

# Needed to create the .deb (all files should be in the binay dir)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/life.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/prio.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)
file(COPY ${PLATFORM_SOURCE_DIR}/config/default_codes/routing.c DESTINATION ${PLATFORM_BINARY_DIR}/config/default_codes)

# Configuration files
install(FILES ${PLATFORM_BINARY_DIR}/config/adtn.ini DESTINATION ${SYSCONFDIR}/${ADTN_CONF_SUBDIR}/)
install(FILES ${PLATFORM_BINARY_DIR}/adtnd/adtnd PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ DESTINATION bin)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/life.c DESTINATION ${SYSCONFDIR}/${ADTN_CONF_SUBDIR}/default_codes)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/prio.c DESTINATION ${SYSCONFDIR}/${ADTN_CONF_SUBDIR}/default_codes)
install(FILES ${PLATFORM_BINARY_DIR}/config/default_codes/routing.c DESTINATION ${SYSCONFDIR}/${ADTN_CONF_SUBDIR}/default_codes)
