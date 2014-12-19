cmake_minimum_required(VERSION 2.8.2)

find_package(Threads)
find_package(OpenSSL)

set(CMAKE_C_FLAGS "-fPIC")

include_directories(include)
add_library(adtn SHARED queue.c rit.c cJSON.c stats.c shm.c minIni.c config.c log.c code.c neighbours.c bundle.c init.c utils.c)
target_link_libraries(adtn ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(adtn ${OPENSSL_LIBRARIES} )
target_link_libraries(adtn rt )
target_link_libraries(adtn m )

install(FILES include/rit.h DESTINATION include)
install(FILES include/queue.h DESTINATION include)

install(FILES ${PLATFORM_SOURCE_DIR}/common/include/bundle.h DESTINATION include)
install(FILES ${PLATFORM_SOURCE_DIR}/common/include/uthash.h DESTINATION include)
install(FILES ${PLATFORM_BINARY_DIR}/common/libadtn.so PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ DESTINATION lib)