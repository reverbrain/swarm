if(BUILD_URLFETCHER)
	locate_library(LIBEV "ev++.h" "ev" "libev")
	include_directories(
	    ${LIBEV_INCLUDE_DIRS}
	)
	link_directories(
	    ${LIBEV_LIBRARY_DIRS}
	)

#	add_executable(crawler crawler.cpp)
#	target_link_libraries(crawler swarm ev)
	add_executable(download download.cpp)
        target_link_libraries(download swarm swarm_urlfetcher ${LIBEV_LIBRARIES} -pthread)
	add_executable(url_test url_test.cpp)
	target_link_libraries(url_test swarm)
        add_executable(server server.cpp)
        target_link_libraries(server thevoid)
endif()
