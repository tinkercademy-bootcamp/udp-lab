#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <set>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

class UDPCountingModel {
private:
    static const char* MULTICAST_IP;
    static const int MULTICAST_PORT = 12345;
    static const int MAX_STUDENTS = 13;
    
    int socket_fd;
    int student_id;
    struct sockaddr_in multicast_addr;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};
    
    std::set<int> received_counts;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_display;
    std::atomic<int> total_received{0};
    std::atomic<int> total_sent{0};
    bool fast_mode = false;

public:
    UDPCountingModel(int id) : socket_fd(-1), student_id(id) {}
    
    ~UDPCountingModel() {
        cleanup();
    }
    
    bool setup() {
        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd < 0) {
            perror("Socket creation failed");
            return false;
        }
        
        int reuse = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            return false;
        }
        
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = htons(MULTICAST_PORT);
        
        if (bind(socket_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
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
        
        multicast_addr.sin_family = AF_INET;
        multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
        multicast_addr.sin_port = htons(MULTICAST_PORT);
        
        std::cout << "UDP Multicast client setup complete for student " << student_id << std::endl;
        std::cout << "Joined multicast group: " << MULTICAST_IP << ":" << MULTICAST_PORT << std::endl;
        std::cout << "I will count when: count % " << MAX_STUDENTS << " == " << student_id << std::endl;
        
        start_time = std::chrono::steady_clock::now();
        last_display = start_time;
        
        return true;
    }
    
    void sendCount(int count) {
        std::string message = "COUNT:" + std::to_string(count);
        
        ssize_t bytes_sent = sendto(socket_fd, message.c_str(), message.length(), 0,
                                   (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
        
        if (bytes_sent < 0) {
            perror("Send failed");
            return;
        }
        
        total_sent++;
        std::cout << "Sent count: " << count << " to multicast group" << std::endl;
    }
    
    void receiveMessages() {
        char buffer[1024];
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        
        while (running) {
            ssize_t bytes_received = recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0,
                                            (struct sockaddr*)&sender_addr, &sender_len);
            
            if (bytes_received <= 0) {
                if (running) {
                    perror("Receive failed");
                }
                continue;
            }
            
            buffer[bytes_received] = '\0';
            
            if (strncmp(buffer, "COUNT:", 6) == 0) {
                int received_count = std::atoi(buffer + 6);
                total_received++;
                
                if (received_counts.find(received_count) != received_counts.end()) {
                    std::cout << "DUPLICATE: Received count " << received_count << " again!" << std::endl;
                } else {
                    received_counts.insert(received_count);
                }
                
                if (received_count >= current_count) {
                    current_count = received_count + 1;
                    
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_display);
                    
                    if (!fast_mode || elapsed.count() >= 500) {
                        displayProgress(received_count, now);
                        last_display = now;
                    }
                    
                    if (elapsed.count() < 100 && !fast_mode) {
                        fast_mode = true;
                        std::cout << "Switching to fast mode (displaying every 500ms)..." << std::endl;
                    }
                } else {
                    if (!fast_mode) {
                        std::cout << "OUT_OF_ORDER: Received count " << received_count 
                                  << " (current: " << current_count << ")" << std::endl;
                    }
                }
            }
        }
    }
    
    void displayProgress(int count, std::chrono::steady_clock::time_point now) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double seconds = elapsed.count() / 1000.0;
        double rate = (seconds > 0) ? total_received / seconds : 0;
        
        int sending_student = count % MAX_STUDENTS;
        
        if (fast_mode) {
            std::cout << "\rReceived count: " << count << " from student " << sending_student
                      << " | Rate: " << std::fixed << std::setprecision(1) << rate << " msgs/sec    ";
            std::cout.flush();
        } else {
            std::cout << "Received count: " << count << " from student " << sending_student
                      << " (next: " << current_count << ") Rate: " 
                      << std::fixed << std::setprecision(1) << rate << " msgs/sec" << std::endl;
        }
    }
    
    void countingLoop() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        while (running) {
            int count_to_send = current_count;
            
            if ((count_to_send % MAX_STUDENTS) == student_id) {
                std::cout << "My turn! Sending count: " << count_to_send << std::endl;
                sendCount(count_to_send);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    void gapDetection() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            if (!fast_mode && received_counts.size() > 10) {
                std::cout << std::endl << "--- Gap Analysis ---" << std::endl;
                
                int max_received = *received_counts.rbegin();
                int gaps = 0;
                
                for (int i = 0; i <= max_received; i++) {
                    if (received_counts.find(i) == received_counts.end()) {
                        std::cout << "GAP: Missing count " << i << std::endl;
                        gaps++;
                        if (gaps >= 5) {
                            std::cout << "... (showing first 5 gaps)" << std::endl;
                            break;
                        }
                    }
                }
                
                if (gaps == 0) {
                    std::cout << "No gaps detected in sequence 0-" << max_received << std::endl;
                }
                
                std::cout << "Total received: " << received_counts.size() 
                          << ", Total sent: " << total_sent << std::endl;
                std::cout << "-------------------" << std::endl << std::endl;
            }
        }
    }
    
    void run() {
        std::thread receive_thread(&UDPCountingModel::receiveMessages, this);
        std::thread counting_thread(&UDPCountingModel::countingLoop, this);
        std::thread gap_thread(&UDPCountingModel::gapDetection, this);
        
        std::string input;
        std::cout << "Commands: 'stats', 'gaps', 'q' to quit" << std::endl;
        while (std::getline(std::cin, input)) {
            if (input == "q" || input == "quit") {
                break;
            } else if (input == "stats") {
                printStatistics();
            } else if (input == "gaps") {
                printGaps();
            }
        }
        
        cleanup();
        
        if (receive_thread.joinable()) receive_thread.join();
        if (counting_thread.joinable()) counting_thread.join();
        if (gap_thread.joinable()) gap_thread.join();
    }
    
    void printStatistics() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double seconds = elapsed.count() / 1000.0;
        
        std::cout << std::endl << "=== Statistics ===" << std::endl;
        std::cout << "Running time: " << std::fixed << std::setprecision(1) << seconds << " seconds" << std::endl;
        std::cout << "Messages received: " << total_received << std::endl;
        std::cout << "Messages sent: " << total_sent << std::endl;
        std::cout << "Unique counts: " << received_counts.size() << std::endl;
        std::cout << "Current count: " << current_count << std::endl;
        std::cout << "Receive rate: " << std::fixed << std::setprecision(1) 
                  << (seconds > 0 ? total_received / seconds : 0) << " msgs/sec" << std::endl;
        std::cout << "==================" << std::endl << std::endl;
    }
    
    void printGaps() {
        if (received_counts.empty()) {
            std::cout << "No messages received yet." << std::endl;
            return;
        }
        
        std::cout << std::endl << "=== Gap Analysis ===" << std::endl;
        int max_received = *received_counts.rbegin();
        int gaps = 0;
        
        for (int i = 0; i <= max_received && gaps < 20; i++) {
            if (received_counts.find(i) == received_counts.end()) {
                std::cout << "Missing count: " << i << std::endl;
                gaps++;
            }
        }
        
        if (gaps == 0) {
            std::cout << "No gaps in sequence 0-" << max_received << std::endl;
        } else if (gaps >= 20) {
            std::cout << "... (showing first 20 gaps)" << std::endl;
        }
        
        std::cout << "===================" << std::endl << std::endl;
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
};

const char* UDPCountingModel::MULTICAST_IP = "239.255.1.1";

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
    
    UDPCountingModel client(student_id);
    
    if (!client.setup()) {
        return 1;
    }
    
    std::cout << std::endl << "=== UDP Multicast Counting (Model Implementation) ===" << std::endl;
    std::cout << "Student ID: " << student_id << std::endl;
    std::cout << "Features: Duplicate detection, gap analysis, statistics" << std::endl;
    std::cout << "=====================================================" << std::endl << std::endl;
    
    client.run();
    
    std::cout << std::endl << "Client shutdown complete." << std::endl;
    return 0;
}