include_directories(
    ${LIBEV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${URIPARSER_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
)

link_directories(
    ${LIBEV_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${URIPARSER_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
)

aux_source_directory(. SWARM_SRC_LIST)
add_library(swarm SHARED ${SWARM_SRC_LIST})
target_link_libraries(swarm curl ev xml2 uriparser)
set_target_properties(swarm PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${SWARM_VERSION_ABI}
    )

install(FILES
	network_url.h
	network_query_list.h
	networkmanager.h
	networkrequest.h
	url_finder.h
	DESTINATION include/swarm/
	)

install(TARGETS swarm
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
