set(SWARM_SRC_LIST
    event_loop.cpp
    event_loop.hpp
    ev_event_loop.cpp
    ev_event_loop.hpp
    boost_event_loop.cpp
    boost_event_loop.hpp
    http_headers.hpp
    http_headers.cpp
    http_request.hpp
    http_request.cpp
    http_response.hpp
    http_response.cpp
    logger.cpp
    logger.hpp
    url_query.cpp
    url_query.hpp
    url.cpp
    url.hpp
    url_finder.cpp
    url_finder.hpp
    )

set(SWARM_HDR_LIST
	event_loop.hpp
	ev_event_loop.hpp
	boost_event_loop.hpp
	http_headers.hpp
	http_request.hpp
	http_response.hpp
	url.hpp
	url_query.hpp
	url_finder.hpp
	logger.hpp
    )

if(BUILD_NETWORK_MANAGER)
    list(APPEND SWARM_SRC_LIST
        networkmanager.cpp
        networkmanager.hpp
        )
    list(APPEND SWARM_HDR_LIST
        networkmanager.hpp
        )
endif()

add_library(swarm SHARED ${SWARM_SRC_LIST})
target_link_libraries(swarm curl ev xml2 uriparser ${Boost_LIBRARIES})
set_target_properties(swarm PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${SWARM_VERSION_ABI}
    )

install(FILES
    ${SWARM_HDR_LIST}
    DESTINATION include/swarm/
    )

install(TARGETS swarm
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
