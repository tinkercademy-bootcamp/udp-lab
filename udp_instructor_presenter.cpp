#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

class UDPInstructorPresenter {
private:
    static const char* MULTICAST_IP;
    static const int MULTICAST_PORT = 12345;
    static const int MAX_STUDENTS = 13;
    
    int socket_fd;
    struct sockaddr_in multicast_addr;
    std::atomic<int> current_count{0};
    std::atomic<int> last_received_count{-1};
    std::atomic<bool> running{true};
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_display;
    std::chrono::steady_clock::time_point last_count_time;
    std::atomic<int> total_counts{0};
    bool fast_mode = false;

public:
    UDPInstructorPresenter() : socket_fd(-1) {}
    
    ~UDPInstructorPresenter() {
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
        
        std::cout << "UDP Instructor Presenter ready" << std::endl;
        std::cout << "Monitoring multicast group: " << MULTICAST_IP << ":" << MULTICAST_PORT << std::endl;
        std::cout << "Will simulate timeouts after 500ms delay" << std::endl << std::endl;
        
        start_time = std::chrono::steady_clock::now();
        last_display = start_time;
        last_count_time = start_time;
        
        return true;
    }
    
    void sendSimulatedCount(int count) {
        std::string message = "COUNT:" + std::to_string(count);
        
        ssize_t bytes_sent = sendto(socket_fd, message.c_str(), message.length(), 0,
                                   (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
        
        if (bytes_sent < 0) {
            perror("Send failed");
            return;
        }
        
        int expected_student = count % MAX_STUDENTS;
        std::cout << std::endl << "[INSTRUCTOR SIMULATION] Count " << count 
                  << " for student " << expected_student << std::endl;
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
                
                if (received_count >= current_count) {
                    last_received_count = received_count;
                    current_count = received_count + 1;
                    total_counts++;
                    last_count_time = std::chrono::steady_clock::now();
                    
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
                }
            }
        }
    }
    
    void displayProgress(int count, std::chrono::steady_clock::time_point now) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double seconds = elapsed.count() / 1000.0;
        double rate = (seconds > 0) ? total_counts / seconds : 0;
        
        int student_id = count % MAX_STUDENTS;
        
        if (fast_mode) {
            std::cout << "\rCurrent count: " << count << " from student " << student_id
                      << " | Rate: " << std::fixed << std::setprecision(1) << rate << " counts/sec    ";
            std::cout.flush();
        } else {
            std::cout << "Count " << count << " from student " << student_id
                      << " (Rate: " << std::fixed << std::setprecision(1) << rate << " counts/sec)" << std::endl;
        }
    }
    
    void timeoutSimulation() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto now = std::chrono::steady_clock::now();
            auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_count_time);
            
            if (since_last.count() > 500) {
                int count_to_simulate = current_count;
                
                sendSimulatedCount(count_to_simulate);
                
                current_count = count_to_simulate + 1;
                total_counts++;
                last_count_time = now;
                
                displayProgress(count_to_simulate, now);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    }
    
    void statisticsDisplay() {
        auto last_stats_time = std::chrono::steady_clock::now();
        int last_total = 0;
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_time);
            
            int current_total = total_counts;
            int counts_this_period = current_total - last_total;
            double period_rate = (elapsed.count() > 0) ? (counts_this_period * 1000.0) / elapsed.count() : 0;
            
            if (!fast_mode) {
                std::cout << std::endl << "--- 10s Statistics ---" << std::endl;
                std::cout << "Total counts: " << current_total << std::endl;
                std::cout << "Last 10s rate: " << std::fixed << std::setprecision(1) << period_rate << " counts/sec" << std::endl;
                std::cout << "Current count: " << last_received_count.load() << std::endl;
                std::cout << "----------------------" << std::endl << std::endl;
            }
            
            last_stats_time = now;
            last_total = current_total;
        }
    }
    
    void run() {
        std::thread receive_thread(&UDPInstructorPresenter::receiveMessages, this);
        std::thread timeout_thread(&UDPInstructorPresenter::timeoutSimulation, this);
        std::thread stats_thread(&UDPInstructorPresenter::statisticsDisplay, this);
        
        std::cout << "=== UDP Multicast Counting Presenter ===" << std::endl;
        std::cout << "Monitoring student counting progress..." << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  'reset' - Reset count to 0" << std::endl;
        std::cout << "  'send X' - Send count X manually" << std::endl;
        std::cout << "  'q' - Quit" << std::endl;
        std::cout << "=========================================" << std::endl << std::endl;
        
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "q" || input == "quit") {
                break;
            } else if (input == "reset") {
                current_count = 0;
                last_received_count = -1;
                total_counts = 0;
                start_time = std::chrono::steady_clock::now();
                last_count_time = start_time;
                std::cout << std::endl << "Count reset to 0" << std::endl;
            } else if (input.substr(0, 5) == "send ") {
                try {
                    int count = std::stoi(input.substr(5));
                    sendSimulatedCount(count);
                } catch (const std::exception& e) {
                    std::cout << "Invalid count format. Use: send <number>" << std::endl;
                }
            }
        }
        
        cleanup();
        
        if (receive_thread.joinable()) receive_thread.join();
        if (timeout_thread.joinable()) timeout_thread.join();
        if (stats_thread.joinable()) stats_thread.join();
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

const char* UDPInstructorPresenter::MULTICAST_IP = "239.255.1.1";

int main() {
    UDPInstructorPresenter presenter;
    
    if (!presenter.setup()) {
        return 1;
    }
    
    presenter.run();
    
    std::cout << std::endl << "Presenter shutdown complete." << std::endl;
    return 0;
}