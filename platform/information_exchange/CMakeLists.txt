cmake_minimum_required (VERSION 2.8.2) 
find_package( Threads )
find_package( OpenSSL )

#RITD 
include_directories(include ${CMAKE_SOURCE_DIR})

add_executable(information_exchange information_exchange.c)
target_link_libraries(information_exchange ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(information_exchange ${OPENSSL_LIBRARIES} )
target_link_libraries(information_exchange adtn)
target_link_libraries(information_exchange rt)

install(TARGETS information_exchange DESTINATION bin)
