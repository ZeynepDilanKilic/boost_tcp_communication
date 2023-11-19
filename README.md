# boost_tcp_communication
# TCPServer

TCPServer is an example of a simple TCP server using Boost.Asio. This project is designed to demonstrate the concepts of network communication and asynchronous programming.

## Installation

To run this project you need the Boost.Asio library. You can download it from [Boost's official website](https://www.boost.org/).

After installation, you can compile the project with this command:

g++ -std=c++11 -I /path/to/boost TCPServer.cpp -o TCPServer

## Configuration
Using the `ServerConfig` structure you can customize the behavior of the server. For example, you can set the maximum number of connections and the timeout period.

## Usage

To start the server:

./TCPServer

