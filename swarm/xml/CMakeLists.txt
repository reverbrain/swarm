set(SWARM_XML_SRC_LIST
    url_finder.cpp
    url_finder.hpp
    )
set(SWARM_XML_HDR_LIST
    url_finder.hpp
    )

add_library(swarm_xml SHARED ${SWARM_XML_SRC_LIST})
target_link_libraries(swarm_xml swarm xml2)

set_target_properties(swarm_xml PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${SWARM_VERSION_ABI}
    )

install(FILES
    ${SWARM_XML_HDR_LIST}
    DESTINATION include/swarm/xml/
    )

install(TARGETS swarm_xml
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
