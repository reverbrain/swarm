find_package(elliptics REQUIRED)

if(BUILD_NETWORK_MANAGER)
#	add_executable(crawler crawler.cpp)
#	target_link_libraries(crawler swarm ev)
	add_executable(download download.cpp)
	target_link_libraries(download swarm swarm_manager ev)
endif()

add_subdirectory(elliptics)
