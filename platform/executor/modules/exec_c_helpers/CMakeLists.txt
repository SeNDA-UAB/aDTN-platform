cmake_minimum_required(VERSION 2.8.2)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

include_directories(${EXECUTOR_SOURCE_DIR} ${PLATFORM_SOURCE_DIR})
add_library(adtnrhelper STATIC adtnrhelper.c)
add_library(adtnrithelper SHARED adtnrithelper.c)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/adtnrhelper.h DESTINATION lib/adtn/exec_helpers/include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/adtnrithelper.h DESTINATION lib/adtn/exec_helpers/include)

install(FILES ${EXECUTOR_BINARY_DIR}/modules/exec_c_helpers/libadtnrhelper.a DESTINATION lib/adtn/exec_helpers)
install(FILES ${EXECUTOR_BINARY_DIR}/modules/exec_c_helpers/libadtnrithelper.so DESTINATION lib)
