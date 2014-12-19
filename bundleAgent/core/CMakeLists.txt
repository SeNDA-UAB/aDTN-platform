cmake_minimum_required (VERSION 2.8.2)

find_package( Threads )
find_package( OpenSSL )

include_directories(include ${CMAKE_SOURCE_DIR} ${EXECUTOR_SOURCE_DIR} ${PLATFORM_SOURCE_DIR})

add_executable(receiver receiver.c)
target_link_libraries(receiver ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(receiver ${OPENSSL_LIBRARIES} )
target_link_libraries(receiver adtn)

add_executable(processor processor.c)
target_link_libraries(processor ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(processor ${OPENSSL_LIBRARIES} )
target_link_libraries(processor adtn)


install(TARGETS receiver DESTINATION bin)
install(TARGETS processor DESTINATION bin)




