#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <csignal>
#include <cstdlib>

class UDPMulticastExample {
public:
    static const char* MULTICAST_IP;
    static const int MULTICAST_PORT = 36702;
    
private:
    int socket_fd;
    struct sockaddr_in multicast_addr;
    std::atomic<bool> running{true};

public:
    UDPMulticastExample() : socket_fd(-1) {}
    
    ~UDPMulticastExample() {
        cleanup();
    }
    
    void cleanup() {
        running = false;
        
        if (socket_fd >= 0) {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
            mreq.imr_interface.s_addr = INADDR_ANY;
            setsockopt(socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
            
            close(socket_fd);
            socket_fd = -1;
        }
    }
    
    bool setup() {
        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd < 0) {
            perror("Socket creation failed");
            return false;
        }
        
        int opt = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            return false;
        }
        
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            perror("setsockopt SO_REUSEPORT failed");
            return false;
        }
        
        // Bind to multicast port
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;
        local_addr.sin_port = htons(MULTICAST_PORT);
        
        if (bind(socket_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            perror("Bind failed");
            return false;
        }
        
        // Join multicast group
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            perror("Failed to join multicast group");
            return false;
        }
        
        // Set up multicast address for sending
        memset(&multicast_addr, 0, sizeof(multicast_addr));
        multicast_addr.sin_family = AF_INET;
        multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
        multicast_addr.sin_port = htons(MULTICAST_PORT);
        
        std::cout << "Connected to multicast group " << MULTICAST_IP << ":" << MULTICAST_PORT << std::endl;
        std::cout << "Type messages to send (Ctrl-C to quit):" << std::endl;
        
        return true;
    }
    
    void sendMessage(const std::string& message) {
        ssize_t bytes_sent = sendto(socket_fd, message.c_str(), message.length(), 0,
                                   (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
        
        if (bytes_sent < 0) {
            perror("Send failed");
        }
    }
    
    void receiveMessages() {
        char buffer[1024];
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        
        while (running) {
            ssize_t bytes_received = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0,
                                            (struct sockaddr*)&sender_addr, &addr_len);
            if (bytes_received <= 0) {
                if (running) {
                    perror("Receive failed");
                }
                break;
            }
            
            buffer[bytes_received] = '\0';
            std::cout << "[received] " << buffer << std::endl;
            std::cout << "> " << std::flush;  // Prompt for next input
        }
    }
    
    void handleUserInput() {
        std::string input;
        while (running && std::getline(std::cin, input)) {
            if (!input.empty()) {
                sendMessage(input);
            }
            std::cout << "> " << std::flush;
        }
    }
    
    void run() {
        std::thread receive_thread(&UDPMulticastExample::receiveMessages, this);
        
        std::cout << "> " << std::flush;
        handleUserInput();
        
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
        
        cleanup();
    }
};

const char* UDPMulticastExample::MULTICAST_IP = "239.255.1.1";

UDPMulticastExample* g_example_ptr = nullptr;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_example_ptr) {
        g_example_ptr->cleanup();
    }
    exit(signal);
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    UDPMulticastExample example;
    g_example_ptr = &example;
    
    if (!example.setup()) {
        return 1;
    }
    
    std::cout << "\n=== UDP Multicast Chat ===" << std::endl;
    std::cout << "Multicast: " << UDPMulticastExample::MULTICAST_IP << ":" << UDPMulticastExample::MULTICAST_PORT << std::endl;
    std::cout << "==========================\n" << std::endl;
    
    example.run();
    
    std::cout << "Example shutdown complete." << std::endl;
    return 0;
}