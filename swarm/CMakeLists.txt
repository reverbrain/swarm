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
