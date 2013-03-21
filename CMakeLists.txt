project(swarm)
cmake_minimum_required(VERSION 2.8)
add_definitions(-std=c++11)

aux_source_directory(lib SWARM_SRC_LIST)
add_library(swarm SHARED ${SWARM_SRC_LIST})
include_directories(/usr/include/libxml2 lib)
target_link_libraries(swarm curl ev xml2 uriparser)

add_executable(crawler example/crawler.cpp)
target_link_libraries(crawler swarm ev)
