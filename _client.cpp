#include <iostream>
#include <string>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <future>
#include <cstdlib>

// An advance version of my network game where the server generates 
// a random number and the client
// tries to guess the number.
//
// Author: Don P
// Date: December 6, 2022

static constexpr int kMaxLineLength = 1024;
static constexpr int kPortNumber = 369;

static constexpr char kWinMessage[] = "You won!\n";
static constexpr char kLowMessage[] = "Your guess is too low.\n";
static constexpr char kHighMessage[] = "Your guess is too high.\n";

// Function to asynchronously read data from the given socket
std::future<std::string> AsyncRead(int sockfd)
{
    return std::async(std::launch::async, [sockfd]()
    {
        array<char, kMaxLineLength> buffer;
        memset(buffer.data(), 0, buffer.size());
        if (read(sockfd, buffer.data(), buffer.size()) == -1)
        {
            throw std::runtime_error(strerror(errno));
        }
        return std::string(buffer.data());
    });
}

// Function to asynchronously write data to the given socket
std::future<void> AsyncWrite(int sockfd, const std::string& data)
{
    return std::async(std::launch::async, [sockfd, data]()
    {
        if (write(sockfd, data.c_str(), data.size()) == -1)
        {
            throw std::runtime_error(strerror(errno));
        }
    });
}

int main()
{
    // Prompt the user for the server hostname and port number
    std::cout << "Enter Server's IP: ";
    std::string hostname;
    std::getline(std::cin, hostname);
    std::cout << "Enter Port: ";
    std::string port_str;
    std::getline(std::cin, port_str);

    // Convert the port number to an integer
    int port = 0;
    try
    {
        port = std::stoi(port_str);
    }
    catch (const std::invalid_argument&)
    {
        // If the port number is not a valid integer, use the default port number
        port = kPortNumber;
    }

    // Create a socket
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        std::cout << "Failed to create socket: " << strerror(errno) << std::endl;
        return 1;
    }

    // Resolve the server hostname or IP address
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* server_addr = nullptr;
    int err = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &server_addr);
    if (err != 0)
    {
        std::cout << "Failed to resolve server address: " << gai_strerror(err) << std::endl;
        return 1;
    }

    // Connect to the server
    if (connect(sockfd, server_addr->ai_addr, server_addr->ai_addrlen) == -1)
    {
        std::cout << "Failed to connect to server:" << strerror(errno) << std::endl;
        return 1;
    }

    // Free the address info struct
    freeaddrinfo(server_addr);

    // Keep playing until the user quits
    while (true)
    {
        // Read data from the server asynchronously
        const auto data = AsyncRead(sockfd).get();

        // Print the data from the server
        std::cout << data;

        // Check if the user has won
        if (data == kWinMessage)
        {
            break;
        }

        // Prompt the user for a guess
        std::cout << "Enter your guess: ";
        std::string guess;
        std::getline(std::cin, guess);

        // Send the user's guess to the server asynchronously
        AsyncWrite(sockfd, guess);
    }

    // Close the socket
    close(sockfd);

    return 0;
}


