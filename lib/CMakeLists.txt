cmake_minimum_required (VERSION 2.8.2)
project(LIB C CXX)

message("-- Starting lib build configuration")

include(../aDTNConfig.cmake.in)

include_directories(${PROJECT_SOURCE_DIR}/../platform)

file(MAKE_DIRECTORY api)
add_subdirectory(api)

install(FILES ${PROJECT_SOURCE_DIR}/api/include/adtn.h DESTINATION include)
install(FILES ${PROJECT_BINARY_DIR}/api/libadtnAPI.so PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE DESTINATION lib)
