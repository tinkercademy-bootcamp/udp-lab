#include <iostream>
#include <chrono>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <thread>

class UDPCountingPresenter {
private:
    static const char* MULTICAST_IP;
    static const int MULTICAST_PORT = 36702;
    int max_students;
    
    int socket_fd;
    struct sockaddr_in multicast_addr;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_display;
    std::chrono::steady_clock::time_point last_count_time;
    std::chrono::steady_clock::time_point last_rate_reset;
    std::atomic<int> total_counts{0};
    std::atomic<int> counts_since_last_reset{0};
    bool fast_mode = false;

public:
    UDPCountingPresenter(int students = 13) : socket_fd(-1), max_students(students) {}
    
    ~UDPCountingPresenter() {
        cleanup();
    }
    
    bool start() {
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
        
        std::cout << "UDP Counting Presenter listening on multicast " << MULTICAST_IP << ":" << MULTICAST_PORT << std::endl;
        std::cout << "Monitoring " << max_students << " students..." << std::endl;
        
        start_time = std::chrono::steady_clock::now();
        last_display = start_time;
        last_count_time = start_time;
        last_rate_reset = start_time;
        counts_since_last_reset = 0;
        
        return true;
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
                continue;
            }
            
            buffer[bytes_received] = '\0';
            
            try {
                int received_count = std::stoi(buffer);
                
                current_count = received_count + 1;
                total_counts++;
                counts_since_last_reset++;
                
                auto now = std::chrono::steady_clock::now();
                last_count_time = now;
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_display);
                
                if (!fast_mode || elapsed.count() >= 500) {
                    displayProgress(received_count, now);
                    last_display = now;
                }
            } catch (const std::exception& e) {
                // Ignore invalid messages
            }
        }
    }
    
    void displayProgress(int count, std::chrono::steady_clock::time_point now) {
        auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double total_seconds = total_elapsed.count() / 1000.0;
        double total_rate = (total_seconds > 0) ? total_counts / total_seconds : 0;
        
        auto current_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rate_reset);
        double current_milliseconds = current_elapsed.count();
        double current_rate = (current_milliseconds > 0) ? counts_since_last_reset / current_milliseconds : 0;
        
        if (fast_mode) {
            std::cout << "\rCurrent count: " << count 
                      << " | Rate: " << std::fixed << std::setprecision(3) << current_rate << " counts/ms    ";
            std::cout.flush();
            last_rate_reset = now;
            counts_since_last_reset = 0;
        } else {            
            std::cout << "\nCount " << count << " from student " << (count % max_students);
            std::cout.flush();

            if (count % max_students == 0 && current_milliseconds > 2000) {
                last_rate_reset = now;
                counts_since_last_reset = 0;
            }
        }

        fast_mode = current_rate > 5.0;
    }
    
    void checkTimeout() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            auto now = std::chrono::steady_clock::now();
            auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_count_time);
            
            if (since_last.count() > 5000) {
                int expected_student = current_count % max_students;
                std::cout << "\n[waiting] Waiting for count " << current_count 
                          << " from student " << expected_student << " (timeout: " 
                          << (since_last.count() / 1000.0) << "s)" << std::endl;
                
                // Reset timer to avoid spamming the message
                last_count_time = now;
            }
        }
    }
    
    void run() {
        std::thread receive_thread(&UDPCountingPresenter::receiveMessages, this);
        std::thread timeout_thread(&UDPCountingPresenter::checkTimeout, this);
        
        std::cout << "Presenter running. Press Ctrl-C to quit..." << std::endl;
        
        if (receive_thread.joinable()) receive_thread.join();
        if (timeout_thread.joinable()) timeout_thread.join();
        
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
};

const char* UDPCountingPresenter::MULTICAST_IP = "239.255.1.1";

UDPCountingPresenter* g_presenter_ptr = nullptr;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_presenter_ptr) {
        g_presenter_ptr->cleanup();
    }
    exit(signal);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <total_students>" << std::endl;
        return 1;
    }
    
    int students = std::atoi(argv[1]);
    if (students <= 0) {
        std::cout << "Error: number of students must be positive" << std::endl;
        return 1;
    }
    
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "Starting UDP Counting Presenter with " << students << " students" << std::endl;
    UDPCountingPresenter presenter(students);
    g_presenter_ptr = &presenter;
    
    if (!presenter.start()) {
        return 1;
    }
    
    presenter.run();
    
    std::cout << std::endl << "Presenter shutdown complete." << std::endl;
    return 0;
}