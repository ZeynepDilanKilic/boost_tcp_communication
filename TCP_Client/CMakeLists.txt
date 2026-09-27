add_executable(TCP_Client main.cpp TCPClient.cpp)
target_link_libraries(TCP_Client PRIVATE tcp_common)
