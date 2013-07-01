project(swarm)
cmake_minimum_required(VERSION 2.6)
add_definitions(-std=c++0x)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
string(REGEX MATCH "([0-9]+\\.[0-9]+)" SWARM_VERSION_ABI "${DEBFULLVERSION}")
string(REGEX MATCH "([0-9]+\\.[0-9]+$)" SWARM_VERSION_MINOR "${DEBFULLVERSION}")

include(cmake/locate_library.cmake)

locate_library(LIBEV "ev++.h" "ev" "libev")
locate_library(CURL "curl.h" "curl" "curl" "libcurl")
locate_library(URIPARSER "Uri.h" "uriparser" "uriparser")
locate_library(LIBXML2 "libxml/xpath.h" "xml2" "libxml2")
find_package(Boost COMPONENTS system thread program_options REQUIRED)


option(BUILD_NETWORK_MANAGER "Build network manager" ON)

include_directories(
    ${LIBEV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${URIPARSER_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

link_directories(
    ${LIBEV_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${URIPARSER_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIRS}
)

add_subdirectory(swarm)
add_subdirectory(thevoid)

option(BUILD_EXAMPLES "Build examples" ON)

if(BUILD_EXAMPLES)
    if(BUILD_NETWORK_MANAGER)
        add_executable(crawler example/crawler.cpp)
        target_link_libraries(crawler swarm ev)
        add_executable(download example/download.cpp)
        target_link_libraries(download swarm ev)
    endif()
    add_executable(elliptics-server example/elliptics-server.cpp)
    target_link_libraries(elliptics-server thevoid elliptics_cpp pthread)
endif()

install(DIRECTORY thevoid/rapidjson
    DESTINATION include/thevoid)
