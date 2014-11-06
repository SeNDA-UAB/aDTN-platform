cmake_minimum_required (VERSION 2.8.2)

find_package( Threads )
find_package( OpenSSL )

#set(CMAKE_C_FLAGS "-fPIC")

include_directories(include ${CMAKE_SOURCE_DIR} ${EXECUTOR_SOURCE_DIR} ${PLATFORM_SOURCE_DIR})

#bundleAgent
add_executable(receiver receiver.c)
target_link_libraries(receiver ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(receiver ${OPENSSL_LIBRARIES} )
target_link_libraries(receiver adtn)

#bundleOutgoing
add_executable(processor processor.c)
target_link_libraries(processor ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(processor ${OPENSSL_LIBRARIES} )
target_link_libraries(processor adtn)
#target_link_libraries(processor dl)


install(TARGETS receiver DESTINATION bin)
install(TARGETS processor DESTINATION bin)




