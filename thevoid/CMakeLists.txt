aux_source_directory(. THEVOID_SRC_LIST)
add_library(thevoid SHARED ${THEVOID_SRC_LIST})

message("THEVOID_SRC_LIST: ${THEVOID_SRC_LIST}")
message("Boost_LIBRARIES: ${Boost_LIBRARIES}")

target_link_libraries(thevoid ${Boost_LIBRARIES} pthread swarm)
set_target_properties(thevoid PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${SWARM_VERSION_ABI}
    )

install(FILES
	server.hpp
	stream.hpp
	streamfactory.hpp
    DESTINATION include/thevoid/
    )

install(TARGETS thevoid
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
