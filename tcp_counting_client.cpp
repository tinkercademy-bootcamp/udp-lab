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

class TCPCountingClient {
private:
    static const char* SERVER_IP;
    static const int SERVER_PORT = 35701;
    
    int socket_fd;
    int student_id;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};

public:
    TCPCountingClient(int id) : socket_fd(-1), student_id(id) {}
    
    ~TCPCountingClient() {
        cleanup();
    }
    
    bool connect() {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            perror("Socket creation failed");
            return false;
        }
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        
        if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
            perror("Invalid address");
            return false;
        }
        
        if (::connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            return false;
        }
        
        std::cout << "Connected to server as student " << student_id << std::endl;
        std::cout << "Waiting for my turn (when count % 13 == " << student_id << ")..." << std::endl;
        
        return true;
    }
    
    void sendCount(int count) {
        std::string message = "COUNT:" + std::to_string(count);
        ssize_t bytes_sent = send(socket_fd, message.c_str(), message.length(), 0);
        
        if (bytes_sent < 0) {
            perror("Send failed");
            return;
        }
        
        std::cout << "Sent count: " << count << std::endl;
    }
    
    void receiveMessages() {
        char buffer[1024];
        
        while (running) {
            ssize_t bytes_received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                if (running) {
                    std::cout << "Connection lost to server" << std::endl;
                }
                break;
            }
            
            buffer[bytes_received] = '\0';
            
            if (strncmp(buffer, "ACCEPTED:", 9) == 0) {
                int accepted_count = std::atoi(buffer + 9);
                current_count = accepted_count + 1;
                
                std::cout << "Server accepted count: " << accepted_count 
                          << " (next expected: " << current_count << ")" << std::endl;
                          
            } else if (strncmp(buffer, "INVALID_COUNT:", 14) == 0) {
                int expected_count = std::atoi(buffer + 14);
                current_count = expected_count;
                
                std::cout << "Invalid count sent. Expected: " << expected_count << std::endl;
            }
        }
    }
    
    void countingLoop() {
        while (running) {
            int count_to_send = current_count;
            
            if ((count_to_send % 13) == student_id) {
                std::cout << "My turn! Sending count: " << count_to_send << std::endl;
                sendCount(count_to_send);
                
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

const char* TCPCountingClient::SERVER_IP = "127.0.0.1";

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <student_id>" << std::endl;
        std::cout << "student_id should be between 0 and 12" << std::endl;
        return 1;
    }
    
    int student_id = std::atoi(argv[1]);
    if (student_id < 0 || student_id > 12) {
        std::cout << "Error: student_id must be between 0 and 12" << std::endl;
        return 1;
    }
    
    TCPCountingClient client(student_id);
    
    if (!client.connect()) {
        return 1;
    }
    
    std::cout << std::endl << "=== TCP Counting Game ===" << std::endl;
    std::cout << "Student ID: " << student_id << std::endl;
    std::cout << "I count when: count % 13 == " << student_id << std::endl;
    std::cout << "=========================" << std::endl << std::endl;
    
    client.run();
    
    std::cout << std::endl << "Client shutdown complete." << std::endl;
    return 0;
}