#include <iostream>
#include <string>
#include <random>
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
#include <memory>

// An advance version of my network game where the server generates 
// a random number and the client
// tries to guess the number.
//
// Author: Don P
// Date: December 6, 2022

// Initialize the random number generator
std::mt19937 rng(std::random_device{}());

// Generate a random number between 1 and 100
std::uniform_int_distribution<int> dist(1, 100);

static constexpr int kMaxLineLength = 1024;
static constexpr int kPortNumber = 369;

static constexpr char kWinMessage[] = "You won!\n";
static constexpr char kLowMessage[] = "Your guess is too low.\n";
static constexpr char kHighMessage[] = "Your guess is too high.\n";

// Represents a socket that can be used to send and receive data
class Socket
{
public:
    // Constructs a socket from a file descriptor
    Socket(int fd) : fd_(fd) {}

    // Receives data from the socket into a buffer
    std::string Receive(size_t size)
    {
        std::array<char, kMaxLineLength> buffer;
        ssize_t num_bytes = recv(fd_, buffer.data(), buffer.size(), 0);
        if (num_bytes == -1)
        {
            throw std::runtime_error(strerror(errno));
        }
        return std::string(buffer.data(), num_bytes);
    }

    // Sends data from a buffer to the socket
    void Send(const std::string& data)
    {
        ssize_t num_bytes = send(fd_, data.data(), data.size(), 0);
        if (num_bytes == -1)
        {
            throw std::runtime_error(strerror(errno));
        }
    }

private:
    int fd_;
};

// Asynchronously listens for incoming connections on the given socket
std::future<std::unique_ptr<Socket>> Listen(int sockfd)
{
    return std::async(std::launch::async, [sockfd]()
    {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int clientfd = accept(sockfd, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
        if (clientfd == -1)
        {
            throw std::runtime_error(strerror(errno));
        }

        return std::make_unique<Socket>(clientfd);
    });
}

// Function to check if the given string represents a valid integer
bool IsValidInteger(const std::string& s)
{
    // Check if the string is empty
    if (s.empty())
    {
        return false;
    }

    // Check if the string contains only digits
    if (s.find_first_not_of("0123456789") != std::string::npos)
{
        return false;
    }

    // The string is a valid integer
    return true;
}

int main()
{
    // Generate the random number
    const int number = dist(rng);

    // Create a socket
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        std::cout << "Failed to create socket: " << strerror(errno) << std::endl;
        return 1;
    }

    // Bind the socket to the given port number
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPortNumber);
    if (bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        std::cout << "Failed to bind socket: " << strerror(errno) << std::endl;
        return 1;
    }

    // Listen for incoming connections on the socket
    if (listen(sockfd, kListenQueueSize) == -1)
    {
        std::cout << "Failed to listen on socket: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Listening for incoming connections on port " << kPortNumber << std::endl;

    // Accept incoming connections and handle them asynchronously
    while (true)
    {
        // Wait for a client to connect
        const auto client_socket = Listen(sockfd).get();

        // Send the welcome message to the client
        client_socket->Send(kWelcomeMessage);

        // Keep track of the number of guesses
        int num_guesses = 0;

        // Keep playing until the client wins or quits
        while (true)
        {
            // Receive a line of input from the client
            const auto user_input = client_socket->Receive(kMaxLineLength);

            // Check if the user entered "QUIT"
            if (user_input == "QUIT")
            {
                break;
            }

            // Check if the user entered a valid integer
            if (!IsValidInteger(user_input))
            {
                client_socket->Send("Invalid input. Please enter a valid integer.\n");
                continue;
            }

            // Convert the input string to an integer
            const int guess = std::stoi(user_input);

            // Increment the number of guesses
            ++num_guesses;

            // Check if the guess is correct
            if (guess == number)
            {
                // Send the win message to the client
                client_socket->Send(kWinMessage);

                // Print a message to the server console
                std::cout << "Client won in " << num_guesses << " guesses.\n";
                break;
            }
            else if (guess < number)
            {
                // Send the low message to the client
                client_socket->Send(kLowMessage);
            }
            else
            {
                // Send the high message to the client
                client_socket->Send(kHighMessage);
            }
        }
    }

    // Close the socket
    close(sockfd);

    return 0;
}

