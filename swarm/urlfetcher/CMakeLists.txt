set(SWARM_ACCESS_MANAGER_SRC_LIST
    event_loop.cpp
    event_loop.hpp
    ev_event_loop.cpp
    ev_event_loop.hpp
    boost_event_loop.cpp
    boost_event_loop.hpp
    url_fetcher.cpp
    url_fetcher.hpp
    )
set(SWARM_ACCESS_MANAGER_HDR_LIST
    event_loop.hpp
    ev_event_loop.hpp
    boost_event_loop.hpp
    url_fetcher.hpp
    )

add_library(swarm_urlfetcher SHARED ${SWARM_ACCESS_MANAGER_SRC_LIST})
target_link_libraries(swarm_urlfetcher swarm curl ev ${Boost_LIBRARIES})

set_target_properties(swarm_urlfetcher PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${SWARM_VERSION_ABI}
    )

install(FILES
    ${SWARM_ACCESS_MANAGER_HDR_LIST}
    DESTINATION include/swarm/urlfetcher/
    )

install(TARGETS swarm_urlfetcher
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
