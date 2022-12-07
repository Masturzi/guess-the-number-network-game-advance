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

// An advance version of my network game where the server generates 
// a random number and the client
// tries to guess the number.
//
// Author: Don P
// Date: December 6, 2022

using std::mt19937;
using std::uniform_int_distribution;
using std::ostream;
using std::cout;
using std::endl;
using std::array;
using std::strerror;
using std::memset;
using std::setw;

static constexpr int kListenQueueSize = 5;
static constexpr int kMaxLineLength = 1024;
static constexpr int kPortNumber = 369;

static constexpr char kWelcomeMessage[] = "Welcome to the guessing game! I'm thinking of a number between 1 and 100. Can you guess it?\n";
static constexpr char kWinMessage[] = "You won!\n";
static constexpr char kLowMessage[] = "Your guess is too low.\n";
static constexpr char kHighMessage[] = "Your guess is too high.\n";

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

int main()
{
    // Initialize the random number generator
    mt19937 rng(std::random_device{}());
    uniform_int_distribution<int> dist(1, 100);
    int number = dist(rng);

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cout << "Failed to create socket: " << strerror(errno) << endl;
        return 1;
    }

    // Bind the socket to an IP address and port
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPortNumber);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        cout << "Failed to bind socket: " << strerror(errno) << endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(sockfd, kListenQueueSize) == -1)
    {
        cout << "Failed to listen on socket: " << strerror(errno) << endl;
        return 1;
    }

    while (true)
    {
        // Accept an incoming connection
        std::unique_ptr<Socket> client = Listen(sockfd).get();

        // Get the client's IP address and port
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        if (getpeername(client->GetFd(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len) == -1)
        {
            cout << "Failed to get client IP address: " << strerror(errno) << endl;
            return 1;
        }

        char client_ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, sizeof(client_ip_str)) == nullptr)
        {
            cout << "Failed to convert client IP address to string: " << strerror(errno) << endl;
            return 1
        }

        int client_port = ntohs(client_addr.sin_port);

        cout << "Accepted connection from " << client_ip_str << ":" << client_port << endl;

        // Send the welcome message to the client
        client->Send(kWelcomeMessage);

        // Play the guessing game with the client
        while (true)
        {
            // Receive a guess from the client
            std::string line = client->Receive(kMaxLineLength);

            // Check if the client has disconnected
            if (line.empty())
            {
                cout << "Client disconnected" << endl;
                break;
            }

            // Check if the line is a valid integer
            if (!IsValidInteger(line))
            {
                cout << "Received invalid input from client: " << line << endl;
                continue;
            }

            // Convert the line to an integer
            int guess = std::stoi(line);

            // Check if the guess is correct
            if (guess == number)
            {
                client->Send(kWinMessage);
                break;
            }

            // Send a message to the client depending on whether their guess was too low or too high
            if (guess < number)
            {
                client->Send(kLowMessage);
            }
            else
            {
                client->Send(kHighMessage);
            }
        }
    }

    return 0;
}
