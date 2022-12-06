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

    // Accept an incoming connection
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int clientfd = accept(sockfd, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
    if (clientfd == -1)
    {
        cout << "Failed to accept connection: " << strerror(errno) << endl;
        return 1;
    }

    // Get the client's IP address and port
    char client_ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, sizeof(client_ip_str)) == nullptr)
    {
        cout << "Failed to convert client IP address to string: " << strerror(errno) << endl;
        return 1;
    }
    int client_port = ntohs(client_addr.sin_port);

    // Print a message indicating that a new client has connected
    cout << "New client connected from " << client_ip_str << ":" << client_port << endl;

    // Send the welcome message to the client
    if (write(clientfd, kWelcomeMessage, sizeof(kWelcomeMessage)) == -1)
    {
        cout << "Failed to send welcome message to client: " << strerror(errno) << endl;
        return 1;
    }

    // Loop until the client guesses the number
    while (true)
    {
        // Read a guess from the client
        array<char, kMaxLineLength> buffer;
        if (read(clientfd, buffer.data(), buffer.size()) == -1)
        {
            cout << "Failed to read from client: " << strerror(errno) << endl;
            return 1;
        }

        // Remove the newline character from the guess
        auto newline = std::find(buffer.begin(), buffer.end(), '\n');
        if (newline != buffer.end())
        {
            *newline = '\0';
        }

        // Check if the guess is a valid integer
        std::string guess_str(buffer.data());
        if (!IsValidInteger(guess_str))
        {
            // If the guess is not a valid integer, ignore it
            continue;
        }

        // Convert the guess to an integer
        int guess = std::stoi(guess_str);

        // Check if the guess is correct
        if (guess == number)
        {
            // Send a message to the client indicating that they won
            if (write(clientfd, kWinMessage, sizeof(kWinMessage)) == -1)
            {
                cout << "Failed to send win message to client: " << strerror(errno) << endl;
                return 1;
            }

            // Close the client socket
            close(clientfd);

            // Exit the loop
            break;
        }
        else if (guess < number)
        {
            // Send a message to the client indicating that their guess is too low
            if (write(clientfd, kLowMessage, sizeof(kLowMessage)) == -1)
            {
                cout << "Failed to send low message to client: " << strerror(errno) << endl;
                return 1;
            }
        }
        else
        {
            // Send a message to the client indicating that their guess is too high
            if (write(clientfd, kHighMessage, sizeof(kHighMessage)) == -1)
            {
                cout << "Failed to send high message to client: " << strerror(errno) << endl;
                return 1;
            }
        }
    }

    // Close the server socket
    close(sockfd);

    return 0;
}

