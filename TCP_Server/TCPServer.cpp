#include "TCPServer.h"

TCPServer::TCPServer(boost::asio::io_service &io_service)
    : m_io_service(io_service), m_acceptor(io_service, tcp::endpoint(tcp::v4(), 12345)),
      m_socket(io_service)
{
    loadConfiguration("ServerConfig.json");
}

TCPServer::~TCPServer()
{
}

void TCPServer::start()
{
    if (!is_running)
    {
        acceptConnection();
        handleCommunication();
        is_running = true;
    }
}

void TCPServer::acceptConnection()
{
    int retry_limit = 5;
    for (int i = 0; i < retry_limit; ++i)
    {
        boost::system::error_code ec;
        m_acceptor.accept(m_socket, ec);

        if (!ec)
        {
            return; // Connection success
        }

        std::cerr << "Connection error, retrying...(" << i + 1 << "/" << retry_limit << "): " << ec.message() << std::endl;
        handleError(ec);
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 1 saniye bekle
    }
}

void TCPServer::handleCommunication()
{
    while (true)
    {
        sendMessageToClient();
        receiveMessageFromClient();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void TCPServer::sendMessageToClient()
{
    std::string buffer;
    size_t size = buffer.size();
    boost::asio::write(m_socket, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(m_socket, boost::asio::buffer(buffer));
}

void TCPServer::receiveMessageFromClient()
{
    boost::system::error_code ec;
    size_t expected_size;

    // Get message size first
    boost::asio::read(m_socket, boost::asio::buffer(&expected_size, sizeof(expected_size)), ec);

    if (!ec && expected_size > 0 && expected_size <= 1024)
    { // Here we set a maximum size limit
        std::vector<char> incoming_data(expected_size);
        boost::asio::read(m_socket, boost::asio::buffer(incoming_data), ec);

        if (!ec)
        {
            std::cout << "Receive message: " << std::string(incoming_data.begin(), incoming_data.end()) << std::endl;
        }
        else
        {
            std::cerr << "When receiving an error message: " << ec.message() << std::endl;
            handleError(ec);
        }
    }
    else
    {
        std::cerr << "Invalid message size or error: " << ec.message() << std::endl;
        handleError(ec);
    }
}

void TCPServer::handleError(const boost::system::error_code &ec)
{
    // Error handling based on user preference
    if (userPrefersRestart(ec))
    {
        restartServer();
        return;
    }

    // Critical error checking
    if (isCriticalError(ec))
    {
        stopServer();
        return;
    }

    // Error frequency check
    if (++error_count > MAX_ERROR_THRESHOLD)
    {
        stopServer();
        return;
    }

   // Restart for certain error types
    if (shouldRestart(ec))
    {
        restartServer();
        return;
    }

    // Default action for other cases
    stopServer();
}

bool TCPServer::isCriticalError(const boost::system::error_code &ec)
{
    // Example: Memory leaks, resource exhaustion, internal server errors, etc.
    return ec == boost::asio::error::no_memory ||
           ec == boost::asio::error::operation_aborted ||
           ec == boost::asio::error::service_not_found;
}

bool TCPServer::shouldRestart(const boost::system::error_code &ec)
{
    // Example: Temporary connection problems, timeout errors, etc.
    return ec == boost::asio::error::connection_refused ||
           ec == boost::asio::error::timed_out ||
           ec == boost::asio::error::host_unreachable;
}

bool TCPServer::userPrefersRestart(const boost::system::error_code &ec)
{
   return serverConfig.restartOnErrors.find(ec.value()) != serverConfig.restartOnErrors.end();
}

void TCPServer::stopServer()
{
    // Stop if server is running
    if (is_running)
    {
        m_socket.close();
        m_acceptor.close();
        is_running = false;
    }
}

void TCPServer::restartServer()
{
    stopServer();
    is_running = false;
    start(); // restart the server
}
void TCPServer::loadConfiguration(const std::string &configFile)
{
    std::ifstream f("ServerConfig.json");
    nlohmann::json j_data;
    f >> j_data;


    // Reading and parsing the configuration file
    // Example: Reading from JSON file and loading into userConfig object
        // JSON verilerini yapıya aktarın
    if (j_data.contains("restartOnErrors") && j_data["restartOnErrors"].is_array()) {
        for (auto& errorCode : j_data["restartOnErrors"]) {
            serverConfig.restartOnErrors.insert(errorCode.get<int>());
        }
    }

    if (j_data.contains("maxConnectionLimit") && j_data["maxConnectionLimit"].is_number()) {
        serverConfig.maxConnectionLimit = j_data["maxConnectionLimit"].get<int>();
    }

    if (j_data.contains("timeoutDuration") && j_data["timeoutDuration"].is_number()) {
        serverConfig.timeoutDuration = j_data["timeoutDuration"].get<int>();
    }

    // Yapılandırmanın doğru okunup okunmadığını kontrol edin
    std::cout << "Max Connection Limit: " << serverConfig.maxConnectionLimit << "\n";
    std::cout << "Timeout Duration: " << serverConfig.timeoutDuration << "\n";
    std::cout << "Restart on Errors: ";
    for (int code : serverConfig.restartOnErrors) {
        std::cout << code << " ";
    }
    std::cout << std::endl;
}