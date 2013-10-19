set(SWARM_SRC_LIST
    event_loop.cpp
    event_loop.h
    ev_event_loop.cpp
    ev_event_loop.h
    boost_event_loop.cpp
    boost_event_loop.h
    logger.cpp
    logger.h
    network_query_list.cpp
    network_query_list.h
    networkrequest.cpp
    networkrequest.h
    network_url.cpp
    network_url.h
    url_finder.cpp
    url_finder.h
    )

set(SWARM_HDR_LIST
	network_url.h
	network_query_list.h
	networkrequest.h
	url_finder.h
	logger.h
    )

if(BUILD_NETWORK_MANAGER)
    list(APPEND SWARM_SRC_LIST
        networkmanager.cpp
        networkmanager.h
        )
    list(APPEND SWARM_HDR_LIST
        networkmanager.h
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
