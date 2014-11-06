cmake_minimum_required(VERSION 2.8.2)

find_package(Threads)
find_package( OpenSSL )

include_directories(include ${CMAKE_SOURCE_DIR})
add_library(adtnAPI SHARED adtn.c)
target_link_libraries(adtnAPI ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(adtnAPI ${OPENSSL_LIBRARIES} )
target_link_libraries(adtnAPI adtn)
