include_directories(${elliptics_INCLUDE_DIRS})
link_directories(${ELLIPTICS_LIBRARY_DIRS})

add_library(elliptics-thevoid SHARED server.cpp)
target_link_libraries(elliptics-thevoid thevoid ${elliptics_cpp_LIBRARY} ${elliptics_client_LIBRARY} pthread)

add_executable(elliptics-server main.cpp)
target_link_libraries(elliptics-server elliptics-thevoid)

install(FILES server.hpp index.hpp DESTINATION include/thevoid/elliptics)
install(TARGETS elliptics-thevoid
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	BUNDLE DESTINATION library
)
