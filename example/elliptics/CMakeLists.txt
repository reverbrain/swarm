include_directories(${elliptics_INCLUDE_DIRS})
link_directories(${ELLIPTICS_LIBRARY_DIRS})

add_executable(elliptics-server server.cpp)
target_link_libraries(elliptics-server thevoid ${elliptics_cpp_LIBRARY} ${elliptics_client_LIBRARY} pthread)

install(FILES index.hpp DESTINATION include/thevoid/elliptics)
