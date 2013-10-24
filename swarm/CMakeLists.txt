set(SWARM_SRC_LIST
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
    )

set(SWARM_HDR_LIST
	http_headers.hpp
	http_request.hpp
	http_response.hpp
	url.hpp
	url_query.hpp
	logger.hpp
    )

add_library(swarm SHARED ${SWARM_SRC_LIST})
target_link_libraries(swarm uriparser ${Boost_LIBRARIES})
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

# Disable for current
# add_subdirectory(xml)

add_subdirectory(manager)
