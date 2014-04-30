project(swarm)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -W -Wall -Wextra")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
string(REGEX MATCH "([0-9]+\\.[0-9]+)" SWARM_VERSION_ABI "${DEBFULLVERSION}")
string(REGEX MATCH "([0-9]+\\.[0-9]+$)" SWARM_VERSION_MINOR "${DEBFULLVERSION}")

include(cmake/locate_library.cmake)

locate_library(LIBEV "ev++.h" "ev" "libev")
locate_library(CURL "curl.h" "curl" "curl" "libcurl")
locate_library(URIPARSER "Uri.h" "uriparser" "uriparser")
locate_library(LIBXML2 "libxml/xpath.h" "xml2" "libxml2")

find_package(Boost COMPONENTS system thread filesystem program_options REQUIRED)

option(BUILD_URLFETCHER "Build url_fetcher" ON)
option(PERF "Build performance checking tool" ON)

include_directories(
    ${CURL_INCLUDE_DIRS}
    ${URIPARSER_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    foreign/blackhole/src
    foreign/blackhole/foreign/rapidjson/include
)

link_directories(
    ${CURL_LIBRARY_DIRS}
    ${URIPARSER_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIRS}
)

add_subdirectory(swarm)
add_subdirectory(thevoid)

option(BUILD_EXAMPLES "Build examples" ON)

if (BUILD_EXAMPLES)
	add_subdirectory(example)
endif()

if (PERF)
	add_subdirectory(perf)
endif()

install(DIRECTORY thevoid/rapidjson
    DESTINATION include/thevoid)

option(WITH_DOXYGEN "Generate documentation by Doxygen" ON)

if(WITH_DOXYGEN)
    message("-- Checking for Doxygen...")
    find_package(Doxygen)
endif()

if(DOXYGEN_FOUND)
    add_subdirectory(doc)
    set(DOC_TARGET "doc")
    configure_file(doc/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    configure_file(doc/DoxygenLayout.xml.in ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml @ONLY)
    add_custom_target(${DOC_TARGET}
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMENT "Building documentation"
        SOURCE doc/Doxyfile.in doc/DoxygenLayout.xml)

#	install( DIRECTORY
#		${CMAKE_CURRENT_BINARY_DIR}/doc/html
#		${CMAKE_CURRENT_BINARY_DIR}/doc/qch
#		DESTINATION share/elliptics/doc
#        )
endif()

