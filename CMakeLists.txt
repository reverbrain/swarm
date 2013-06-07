project(swarm)
cmake_minimum_required(VERSION 2.8)
add_definitions(-std=c++0x)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" SWARM_VERSION_ABI "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+$)" SWARM_VERSION_MINOR "${DEBFULLVERSION}")

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
    lib
)

LINK_DIRECTORIES(
    ${LIBEV_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${URIPARSER_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
)

aux_source_directory(lib SWARM_SRC_LIST)
add_library(swarm SHARED ${SWARM_SRC_LIST})
target_link_libraries(swarm curl ev xml2 uriparser)
set_target_properties(swarm PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${SWARM_VERSION_ABI}
    )

option(BUILD_EXAMPLES "Build examples, needs >=gcc-4.6" OFF)

if(BUILD_EXAMPLES)
    add_executable(crawler example/crawler.cpp)
    target_link_libraries(crawler swarm ev)
    add_executable(download example/download.cpp)
    target_link_libraries(download swarm ev)
endif()

install(FILES
	lib/network_url.h
	lib/networkmanager.h
	lib/networkrequest.h
	lib/url_finder.h
	DESTINATION include/swarm/
	)
install(TARGETS swarm
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
    )
