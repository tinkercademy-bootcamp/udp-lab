#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <netdb.h>

class TCPCountingClient {
private:
    std::string server_ip;
    static const int SERVER_PORT = 35701;
    
    int socket_fd;
    int student_id;
    int total_students;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};

public:
    TCPCountingClient(int id, int total, const std::string& ip) 
        : socket_fd(-1), student_id(id), total_students(total), server_ip(ip) {}
    
    ~TCPCountingClient() {
        cleanup();
    }
    
    bool connect() {
        // Don't create the socket here - we'll create it in the getaddrinfo loop
        socket_fd = -1;
        
        // Use getaddrinfo which handles both IP addresses and hostnames in a more modern way
        struct addrinfo hints, *server_info, *p;
        int rv;
        
        // Set up hints structure
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;     // Use IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP
        
        // Convert port to string
        std::string port_str = std::to_string(SERVER_PORT);
        
        // Get address info
        std::cout << "Resolving " << server_ip << "..." << std::endl;
        if ((rv = getaddrinfo(server_ip.c_str(), port_str.c_str(), &hints, &server_info)) != 0) {
            std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
            return false;
        }
        
        // Loop through all the results and connect to the first one we can
        for (p = server_info; p != nullptr; p = p->ai_next) {
            // Create socket
            if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("Client: socket");
                continue;
            }
            
            // Set socket timeout
            struct timeval tv;
            tv.tv_sec = 5;  // 5 second timeout
            tv.tv_usec = 0;
            if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                perror("Socket timeout option");
                // Continue anyway
            }
            
            std::cout << "Connecting to " << server_ip << ":" << SERVER_PORT << "..." << std::endl;
            if (::connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
                close(socket_fd);
                socket_fd = -1;
                perror("Client: connect");
                continue;
            }
            
            break; // If we get here, we connected successfully
        }
        
        // Free the server info structure
        freeaddrinfo(server_info);
        
        // Check if we connected to anything
        if (p == nullptr) {
            std::cerr << "Failed to connect to " << server_ip << ":" << SERVER_PORT << std::endl;
            if (socket_fd != -1) {
                close(socket_fd);
                socket_fd = -1;
            }
            return false;
        }
        
        std::cout << "Connected to server at " << server_ip << " as student " << student_id << std::endl;
        std::cout << "Waiting for my turn (when count % " << total_students << " == " << student_id << ")..." << std::endl;
        
        return true;
    }
    
    void sendCount(int count) {
        std::string message = std::to_string(count) + "\n";
        ssize_t bytes_sent = send(socket_fd, message.c_str(), message.length(), 0);
        
        if (bytes_sent < 0) {
            perror("Send failed");
            return;
        }
        
        std::cout << "Sent count: " << count << std::endl;
    }
    
    void receiveMessages() {
        char buffer[1024];
        std::string partial_buffer;
        
        while (running) {
            ssize_t bytes_received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                if (running) {
                    std::cout << "Connection lost to server" << std::endl;
                }
                break;
            }
            
            buffer[bytes_received] = '\0';
            
            // Add received data to our partial buffer
            partial_buffer += buffer;
            
            // Process complete lines
            size_t pos;
            while ((pos = partial_buffer.find('\n')) != std::string::npos) {
                // Extract line
                std::string line = partial_buffer.substr(0, pos);
                partial_buffer = partial_buffer.substr(pos + 1);
                
                // Try to parse as a number
                try {
                    int received_count = std::stoi(line);
                    current_count = received_count + 1;
                    
                    std::cout << "Received count: " << received_count 
                              << " (next expected: " << current_count << ")" << std::endl;
                } catch (const std::exception& e) {
                    // Ignore invalid lines
                }
            }
        }
    }
    
    void countingLoop() {
        while (running) {
            int count_to_send = current_count;
            
            if ((count_to_send % total_students) == student_id) {
                std::cout << "My turn! Sending count: " << count_to_send << std::endl;
                sendCount(count_to_send);
                
                // No need to increment here - will be updated when we receive the broadcast
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
    
    void run() {
        std::thread receive_thread(&TCPCountingClient::receiveMessages, this);
        std::thread counting_thread(&TCPCountingClient::countingLoop, this);
        
        std::string input;
        std::cout << "Press 'q' and Enter to quit..." << std::endl;
        while (std::getline(std::cin, input)) {
            if (input == "q" || input == "quit") {
                break;
            }
        }
        
        cleanup();
        
        if (receive_thread.joinable()) receive_thread.join();
        if (counting_thread.joinable()) counting_thread.join();
    }
    
    void cleanup() {
        running = false;
        
        if (socket_fd >= 0) {
            close(socket_fd);
            socket_fd = -1;
        }
    }
};


int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cout << "Usage: " << argv[0] << " <student_id> <total_students> [hostname]" << std::endl;
        std::cout << "student_id should be between 0 and (total_students-1)" << std::endl;
        std::cout << "hostname can be either an IP address or hostname (defaults to trainers-in.tnkr.be)" << std::endl;
        return 1;
    }
    
    int student_id = std::atoi(argv[1]);
    int total_students = std::atoi(argv[2]);
    std::string hostname = (argc == 4) ? argv[3] : "trainers-in.tnkr.be";
    
    if (student_id < 0 || student_id >= total_students) {
        std::cout << "Error: student_id must be between 0 and " << (total_students-1) << std::endl;
        return 1;
    }
    
    TCPCountingClient client(student_id, total_students, hostname);
    
    if (!client.connect()) {
        return 1;
    }
    
    std::cout << std::endl << "=== TCP Counting Game ===" << std::endl;
    std::cout << "Student ID: " << student_id << std::endl;
    std::cout << "Server: " << hostname << std::endl;
    std::cout << "I count when: count % " << total_students << " == " << student_id << std::endl;
    std::cout << "=========================" << std::endl << std::endl;
    
    client.run();
    
    std::cout << std::endl << "Client shutdown complete." << std::endl;
    return 0;
}