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

// An advance version of my network game where the server generates 
// a random number and the client
// tries to guess the number.
//
// Author: Don P
// Date: December 6, 2022

using std::ostream;
using std::cin;
using std::cout;
using std::endl;
using std::array;
using std::strerror;
using std::memset;
using std::setw;

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
    cout << "Enter Servers IP: ";
    std::string hostname;
    std::getline(cin, hostname);
    cout << "Enter Port: ";
    std::string port_str;
    std::getline(cin, port_str);

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
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cout << "Failed to create socket: " << strerror(errno) << endl;
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
        cout << "Failed to resolve server address: " << gai_strerror(err) << endl;
        return 1;
    }

    // Connect to the server
    if (connect(sockfd, server_addr->ai_addr, server_addr->ai_addrlen) == -1)
    {
        cout << "Failed to connect to server:" << strerror(errno) << endl;
        return 1;
    }

    // Loop until the player wins the game
    while (true)
    {
        // Prompt the user for a guess
        cout << "Enter your guess: ";

        // Read the guess from the standard input
        array<char, kMaxLineLength> buffer;
        if (!cin.getline(buffer.data(), buffer.size()))
        {
            // If the input stream is closed, exit the loop
            break;
        }

        // Send the guess to the server
        auto write_future = AsyncWrite(sockfd, buffer.data());
        try
        {
            write_future.get();
        }
        catch (const std::exception& e)
        {
            cout << "Failed to send guess to server: " << e.what() << endl;
            return 1;
        }

        // Read the server's response
        auto read_future = AsyncRead(sockfd);
        try
        {
            std::string response = read_future.get();

            // Check the server's response
            if (response == kWinMessage)
            {
                cout << response;
                break;
            }
            else
            {
                cout << response;
            }
        }
        catch (const std::exception& e)
        {
            cout << "Failed to read from server: " << e.what() << endl;
            return 1;
        }
    }

    return 0;
}
