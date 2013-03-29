project(swarm)
cmake_minimum_required(VERSION 2.8)
add_definitions(-std=c++0x)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(CURL "curl.h" "curl" "curl" "libcurl")
LOCATE_LIBRARY(URIPARSER "Uri.h" "uriparser" "uriparser")
LOCATE_LIBRARY(LIBXML2 "libxml/xpath.h" "xml2" "libxml2")

INCLUDE_DIRECTORIES(
    ${LIBEV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${URIPARSER_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
    ${LIBEV_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${URIPARSER_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
)

aux_source_directory(lib SWARM_SRC_LIST)
add_library(swarm SHARED ${SWARM_SRC_LIST})
include_directories(/usr/include/libxml2 lib)
target_link_libraries(swarm curl ev xml2 uriparser)
set_target_properties(swarm PROPERTIES
    VERSION "0.1.0.0"
    SOVERSION "0.1"
    )

add_executable(crawler example/crawler.cpp)
target_link_libraries(crawler swarm ev)

install(FILES
	lib/network_url.h
	lib/networkmanager.h
	lib/url_finder.h
	DESTINATION include/swarm/
	)
install(TARGETS swarm
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
