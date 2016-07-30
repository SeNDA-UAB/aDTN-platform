cmake_minimum_required (VERSION 2.8.2)

project(EXECUTOR C)
find_package(Threads)

add_executable(executor executor.c ebpf/exec.c)

include_directories(ebpf/vm/ ebpf/vm/inc/)

# Build ubpf vm
add_custom_target(libubpf COMMAND make WORKING_DIRECTORY
	${CMAKE_CURRENT_LIST_DIR}/ebpf/vm/)
add_dependencies(executor libubpf)

target_link_libraries(executor ${CMAKE_CURRENT_LIST_DIR}/ebpf/vm/libubpf.a)
target_link_libraries(executor ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(executor dl)
target_link_libraries(executor adtn)

install(TARGETS executor DESTINATION bin)
