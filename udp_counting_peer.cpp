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
#include <csignal>
#include <cstdlib>
#include <cerrno>

class UDPCountingPeer {
public:
    static const char* MULTICAST_IP;
    static const int MULTICAST_PORT = 36702;
    
private:
    
    int socket_fd;
    struct sockaddr_in multicast_addr;
    int student_id;
    int total_students;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};
    std::chrono::steady_clock::time_point last_count_time;
    
public:
    UDPCountingPeer(int id, int total) : socket_fd(-1), student_id(id), total_students(total) {}
    
    ~UDPCountingPeer() {
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
    
    bool connect() {
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
        
        // Increase send buffer size to help prevent buffer overflow
        int send_buffer_size = 1024 * 1024; // 1MB
        if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) < 0) {
            perror("Warning: Failed to set send buffer size");
            // Continue anyway, this is not critical
        }
        
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;
        local_addr.sin_port = htons(MULTICAST_PORT);
        
        if (bind(socket_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            perror("Bind failed");
            return false;
        }
        
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            perror("Failed to join multicast group");
            return false;
        }
        
        memset(&multicast_addr, 0, sizeof(multicast_addr));
        multicast_addr.sin_family = AF_INET;
        multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
        multicast_addr.sin_port = htons(MULTICAST_PORT);
        
        std::cout << "Connected to multicast group " << MULTICAST_IP << ":" << MULTICAST_PORT << " as student " << student_id << std::endl;
        std::cout << "Waiting for my turn (when count % " << total_students << " == " << student_id << ")..." << std::endl;
        
        last_count_time = std::chrono::steady_clock::now();
        
        return true;
    }
    
    void sendCount(int count) {
        std::string message = std::to_string(count);
        
        ssize_t bytes_sent = sendto(socket_fd, message.c_str(), message.length(), 0,
                                   (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
        
        if (bytes_sent < 0) {
            if (errno == ENOBUFS) {
                std::cout << "\n[warning] Network buffer full, retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                // Retry once
                bytes_sent = sendto(socket_fd, message.c_str(), message.length(), 0,
                                   (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
                if (bytes_sent < 0) {
                    perror("Send failed after retry");
                }
            } else {
                perror("Send failed");
            }
            return;
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
            
            try {
                int received_count = std::stoi(buffer);
                current_count = received_count + 1;
                last_count_time = std::chrono::steady_clock::now();
                
                if (current_count <= 20) {
                    std::cout << "Received count: " << received_count 
                              << " (next expected: " << current_count << ")" << std::endl;
                }
                
                if ((current_count % total_students) == student_id) {
                    if (current_count <= 20) {
                        std::cout << "My turn! Sending count: " << current_count << std::endl;
                    }
                    sendCount(current_count);
                }
            } catch (const std::exception& e) {
                // Ignore invalid messages
            }
        }
    }
    
    void handleInitialTurn() {
        if (student_id == 0 && current_count == 0) {
            std::cout << "Starting the game! Sending count: 0" << std::endl;
            sendCount(0);
            last_count_time = std::chrono::steady_clock::now();
        }
    }
    
    void checkTimeout() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            if (student_id == 0) {
                auto now = std::chrono::steady_clock::now();
                auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_count_time);
                
                if (since_last.count() > 8000) {
                    std::cout << "\n[timeout] No activity for 8 seconds. Restarting from 0..." << std::endl;
                    current_count = 0;
                    sendCount(0);
                    last_count_time = now;
                }
            }
        }
    }
    
    void run() {
        handleInitialTurn();
        
        std::thread receive_thread(&UDPCountingPeer::receiveMessages, this);
        std::thread timeout_thread(&UDPCountingPeer::checkTimeout, this);
        
        std::cout << "Peer running. Press Ctrl-C to quit..." << std::endl;
        std::cout << "Will enter silent mode after receiving 20 counts." << std::endl;
        if (student_id == 0) {
            std::cout << "As student 0, I will restart counting if no activity for 5 seconds." << std::endl;
        }
        
        if (receive_thread.joinable()) receive_thread.join();
        if (timeout_thread.joinable()) timeout_thread.join();
        
        cleanup();
    }
};

const char* UDPCountingPeer::MULTICAST_IP = "239.255.1.1";

UDPCountingPeer* g_peer_ptr = nullptr;

void signal_handler(int signal) {
    if (g_peer_ptr) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    }
    
    if (g_peer_ptr) {
        g_peer_ptr->cleanup();
    }
    exit(signal);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <student_id> <total_students>" << std::endl;
        std::cout << "student_id should be between 0 and (total_students-1)" << std::endl;
        return 1;
    }
    
    int student_id = std::atoi(argv[1]);
    int total_students = std::atoi(argv[2]);
    
    if (student_id < 0 || student_id >= total_students) {
        std::cout << "Error: student_id must be between 0 and " << (total_students-1) << std::endl;
        return 1;
    }
    
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    UDPCountingPeer peer(student_id, total_students);
    g_peer_ptr = &peer;
    
    if (!peer.connect()) {
        return 1;
    }
    
    std::cout << std::endl << "=== UDP Counting Game ===" << std::endl;
    std::cout << "Student ID: " << student_id << std::endl;
    std::cout << "Multicast: " << UDPCountingPeer::MULTICAST_IP << ":" << UDPCountingPeer::MULTICAST_PORT << std::endl;
    std::cout << "I count when: count % " << total_students << " == " << student_id << std::endl;
    std::cout << "=========================" << std::endl << std::endl;
    
    peer.run();
    
    std::cout << std::endl << "Peer shutdown complete." << std::endl;
    return 0;
}