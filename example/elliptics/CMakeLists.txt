find_package(Elliptics 2.24 REQUIRED)

include_directories(${ELLIPTICS_INCLUDE_DIRS})

set(SWARM_ELLIPTICS_HEADERS
	asio.hpp
	common.hpp
        elliptics_auth.hpp
	elliptics_logger.hpp
	elliptics_cache.hpp
	index.hpp
	io.hpp
	jsonvalue.hpp
	server.hpp
	signature_base.hpp
)

set(SWARM_ELLIPTICS_SOURCES
        elliptics_auth.cpp
	elliptics_cache.cpp
	server.cpp
	signature_base.cpp
)

add_library(elliptics-thevoid SHARED ${SWARM_ELLIPTICS_SOURCES} ${SWARM_ELLIPTICS_HEADERS})
target_link_libraries(elliptics-thevoid thevoid ${ELLIPTICS_LIBRARIES} pthread)
set_target_properties(elliptics-thevoid PROPERTIES
	VERSION ${DEBFULLVERSION}
	SOVERSION ${SWARM_VERSION_ABI}
)

add_executable(elliptics-server main.cpp)
target_link_libraries(elliptics-server elliptics-thevoid)

install(FILES
	asio.hpp
	common.hpp
	elliptics_logger.hpp
	elliptics_cache.hpp
	index.hpp
	io.hpp
	jsonvalue.hpp
	server.hpp
	signature_base.hpp
	DESTINATION include/thevoid/elliptics)
install(TARGETS elliptics-thevoid
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	BUNDLE DESTINATION library
)
