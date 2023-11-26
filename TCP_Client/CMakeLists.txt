cmake_minimum_required(VERSION 3.27.0)
project(TCP_Server VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

set(Boost_USE_STATIC_LIBS ON)

set(SOURCE_FILES main.cpp TCPClient.cpp)
set(HEADER_FILES TCPClient.h ../nlohmann-json/json.hpp)

#add_executable(TCP_Server main.cpp TCPServer.cpp TCPServer.h)
add_executable(TCP_Client ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(TCP_Client ${Boost_LIBRARIES} ws2_32)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
