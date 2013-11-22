find_package(Boost REQUIRED COMPONENTS system program_options)

INCLUDE(${PROJECT_SOURCE_DIR}/cmake/locate_library.cmake)
locate_library(LIBEV "ev++.h" "ev" "libev")

include_directories(${PROJECT_SOURCE_DIR}/include
	${LIBEV_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${LIBEV_LIBRARY_DIRS}
	${Boost_LIBRARY_DIRS}
)

add_executable(swarm_perf_server server.cpp)
target_link_libraries(swarm_perf_server
	${Boost_LIBRARIES}
	swarm swarm_urlfetcher thevoid
	)

add_executable(swarm_perf_client client.cpp)
target_link_libraries(swarm_perf_client
	${LIBEV_LIBRARIES}
	${Boost_LIBRARIES}
	swarm swarm_urlfetcher
        -pthread
)

FILE(GLOB headers
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
install(FILES ${headers} DESTINATION include/swarm/perf)
install(TARGETS swarm_perf_server swarm_perf_client
	RUNTIME DESTINATION bin COMPONENT runtime)
