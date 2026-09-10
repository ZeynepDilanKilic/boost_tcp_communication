add_executable(TCP_Server main.cpp TCPServer.cpp)
target_link_libraries(TCP_Server PRIVATE tcp_common)
