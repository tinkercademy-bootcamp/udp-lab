#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

class UDPCountingClient {
private:
    static const char* MULTICAST_IP;
    static const int MULTICAST_PORT = 12345;
    static const int MAX_STUDENTS = 13;
    
    int socket_fd;
    int student_id;
    struct sockaddr_in multicast_addr;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};

public:
    UDPCountingClient(int id) : socket_fd(-1), student_id(id) {}
    
    ~UDPCountingClient() {
        cleanup();
    }
    
    bool setup() {
        // TODO: Create UDP socket
        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd < 0) {
            perror("Socket creation failed");
            return false;
        }
        
        // TODO: Set socket options for multicast
        int reuse = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            return false;
        }
        
        // TODO: Bind to multicast address
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
        bind_addr.sin_port = htons(MULTICAST_PORT);
        
        if (bind(socket_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            perror("Bind failed");
            return false;
        }
        
        // TODO: Join multicast group
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            perror("Failed to join multicast group");
            return false;
        }
        
        // TODO: Setup multicast destination address for sending
        multicast_addr.sin_family = AF_INET;
        multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
        multicast_addr.sin_port = htons(MULTICAST_PORT);
        
        std::cout << "UDP Multicast client setup complete for student " << student_id << std::endl;
        std::cout << "Joined multicast group: " << MULTICAST_IP << ":" << MULTICAST_PORT << std::endl;
        std::cout << "I will count when: count % " << MAX_STUDENTS << " == " << student_id << std::endl;
        
        return true;
    }
    
    void sendCount(int count) {
        // TODO: Send count message to multicast group
        std::string message = "COUNT:" + std::to_string(count);
        
        ssize_t bytes_sent = sendto(socket_fd, message.c_str(), message.length(), 0,
                                   (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
        
        if (bytes_sent < 0) {
            perror("Send failed");
            return;
        }
        
        std::cout << "Sent count: " << count << " to multicast group" << std::endl;
    }
    
    void receiveMessages() {
        char buffer[1024];
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        
        while (running) {
            // TODO: Receive messages from multicast group
            ssize_t bytes_received = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0,
                                            (struct sockaddr*)&sender_addr, &sender_len);
            
            if (bytes_received <= 0) {
                if (running) {
                    perror("Receive failed");
                }
                continue;
            }
            
            buffer[bytes_received] = '\0';
            
            // TODO: Parse received messages
            if (strncmp(buffer, "COUNT:", 6) == 0) {
                int received_count = std::atoi(buffer + 6);
                
                // TODO: Update current count based on received message
                // Hint: You might want to handle out-of-order messages
                if (received_count >= current_count) {
                    current_count = received_count + 1;
                    std::cout << "Received count: " << received_count 
                              << " (next expected: " << current_count << ")" << std::endl;
                } else {
                    std::cout << "Received old count: " << received_count 
                              << " (current: " << current_count << ")" << std::endl;
                }
            }
        }
    }
    
    void countingLoop() {
        // Initial delay to let everyone start up
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        while (running) {
            int count_to_send = current_count;
            
            // TODO: Check if it's my turn to count
            if ((count_to_send % MAX_STUDENTS) == student_id) {
                std::cout << "My turn! Sending count: " << count_to_send << std::endl;
                sendCount(count_to_send);
                
                // TODO: Add appropriate delay to avoid flooding
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                // TODO: Wait and check again
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
    
    void run() {
        std::thread receive_thread(&UDPCountingClient::receiveMessages, this);
        std::thread counting_thread(&UDPCountingClient::countingLoop, this);
        
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
            // TODO: Leave multicast group before closing
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
            mreq.imr_interface.s_addr = INADDR_ANY;
            
            setsockopt(socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
            
            close(socket_fd);
            socket_fd = -1;
        }
    }
};

const char* UDPCountingClient::MULTICAST_IP = "239.255.1.1";

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
    
    UDPCountingClient client(student_id);
    
    if (!client.setup()) {
        return 1;
    }
    
    std::cout << std::endl << "=== UDP Multicast Counting Game ===" << std::endl;
    std::cout << "Student ID: " << student_id << std::endl;
    std::cout << "Multicast Group: " << UDPCountingClient::MULTICAST_IP << ":" << 12345 << std::endl;
    std::cout << "=================================" << std::endl << std::endl;
    
    client.run();
    
    std::cout << std::endl << "Client shutdown complete." << std::endl;
    return 0;
}

/*
 * STUDENT EXERCISES:
 * 
 * 1. Basic Implementation:
 *    - Review the TODO comments and ensure you understand each step
 *    - Compile and test with: g++ -o udp_counter udp_counting_skeleton.cpp
 *    - Run with: ./udp_counter <your_student_id>
 * 
 * 2. Test with Command Line:
 *    While others are coding, test by sending messages manually:
 *    echo "COUNT:7" | socat - UDP4-DATAGRAM:239.255.1.1:12345
 * 
 * 3. Observe Issues:
 *    - What happens if two students send the same count?
 *    - Do messages always arrive in order?
 *    - What happens if packets are lost?
 * 
 * 4. Enhanced Features (Optional):
 *    - Add sequence number validation
 *    - Implement duplicate detection
 *    - Add timeout for stuck counts
 *    - Display statistics (counts/second)
 * 
 * 5. Debugging:
 *    - Use tcpdump to monitor traffic: sudo tcpdump -i any host 239.255.1.1
 *    - Check multicast routes: ip route show | grep 239
 */