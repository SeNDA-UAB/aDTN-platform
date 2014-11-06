cmake_minimum_required (VERSION 2.8.2) 

#queue 
include_directories(include ${CMAKE_SOURCE_DIR})

add_executable(queueManager queueManager.c)
target_link_libraries(queueManager adtn)

#add_executable(newqueue newqueue.c)
#target_link_libraries(newqueue adtn)
#target_link_libraries(newqueue ${CMAKE_THREAD_LIBS_INIT})

include_directories(../common/include ../common/  ${CMAKE_SOURCE_DIR})

if(NOT "${TEST_DATA_PATH}" STREQUAL "")
	add_definitions( -DQTEST_DATA_PATH="${TEST_DATA_PATH}" )
endif()

if(NOT "${TEST_SOCK_NAME}" STREQUAL "" )
	add_definitions ( -DQTEST_SOCK_NAME="${TEST_SOCK_NAME}" )
endif()

STRING(TOLOWER "${PROF}" PROF)
if(PROF STREQUAL "true")
	MESSAGE("-- [INFO] Including test from ${CMAKE_CURRENT_SOURCE_DIR}")
	add_executable(queueTests queueTest.cc)
	target_link_libraries(queueTests ${GTEST_LIBRARIES} pthread adtn)

    add_executable(queueSTests queueSTest.cc)
    target_link_libraries(queueSTests ${GTEST_LIBRARIES} pthread adtn)
else()
	MESSAGE("-- [INFO] Not including test from ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

install(TARGETS queueManager DESTINATION bin)
#install(TARGETS newqueue DESTINATION bin)
