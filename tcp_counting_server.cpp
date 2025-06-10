#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <atomic>

class TCPCountingServer {
private:
    static const int PORT = 35701;
    static const int MAX_STUDENTS = 13;
    
    int server_fd;
    std::vector<int> client_sockets;
    std::mutex clients_mutex;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_display;
    std::atomic<int> total_counts{0};
    bool fast_mode = false;

public:
    TCPCountingServer() : server_fd(-1) {
        client_sockets.reserve(MAX_STUDENTS);
    }
    
    ~TCPCountingServer() {
        cleanup();
    }
    
    bool start() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            perror("Socket creation failed");
            return false;
        }
        
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt failed");
            return false;
        }
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            perror("Bind failed");
            return false;
        }
        
        if (listen(server_fd, MAX_STUDENTS) < 0) {
            perror("Listen failed");
            return false;
        }
        
        std::cout << "TCP Counting Server listening on port " << PORT << std::endl;
        std::cout << "Waiting for students to connect..." << std::endl;
        
        start_time = std::chrono::steady_clock::now();
        last_display = start_time;
        
        return true;
    }
    
    void acceptClients() {
        while (running && client_sockets.size() < MAX_STUDENTS) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_socket < 0) {
                if (running) {
                    perror("Accept failed");
                }
                continue;
            }
            
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets.push_back(client_socket);
            
            std::cout << "Student " << (client_sockets.size() - 1) 
                      << " connected. Total: " << client_sockets.size() 
                      << "/" << MAX_STUDENTS << std::endl;
            
            std::thread(&TCPCountingServer::handleClient, this, client_socket, client_sockets.size() - 1).detach();
        }
    }
    
    void handleClient(int client_socket, int student_id) {
        char buffer[1024];
        
        while (running) {
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                break;
            }
            
            buffer[bytes_received] = '\0';
            
            if (strncmp(buffer, "COUNT:", 6) == 0) {
                int received_count = std::atoi(buffer + 6);
                
                if (received_count == current_count && (received_count % MAX_STUDENTS) == student_id) {
                    current_count++;
                    total_counts++;
                    
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
                    
                    broadcastCount(received_count, student_id);
                } else {
                    std::string error_msg = "INVALID_COUNT:" + std::to_string(current_count);
                    send(client_socket, error_msg.c_str(), error_msg.length(), 0);
                }
            }
        }
        
        close(client_socket);
    }
    
    void displayProgress(int count, std::chrono::steady_clock::time_point now) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double seconds = elapsed.count() / 1000.0;
        double rate = (seconds > 0) ? total_counts / seconds : 0;
        
        if (fast_mode) {
            std::cout << "\rCurrent count: " << count 
                      << " | Rate: " << std::fixed << std::setprecision(1) << rate << " counts/sec    ";
            std::cout.flush();
        } else {
            std::cout << "Count " << count << " from student " << (count % MAX_STUDENTS) 
                      << " (Rate: " << std::fixed << std::setprecision(1) << rate << " counts/sec)" << std::endl;
        }
    }
    
    void broadcastCount(int count, int from_student) {
        std::string message = "ACCEPTED:" + std::to_string(count);
        
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int socket : client_sockets) {
            send(socket, message.c_str(), message.length(), 0);
        }
    }
    
    void simulateTimeouts() {
        auto last_count_time = std::chrono::steady_clock::now();
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            auto now = std::chrono::steady_clock::now();
            auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_count_time);
            
            if (since_last.count() > 1000 && client_sockets.size() > 0) {
                int expected_student = current_count % MAX_STUDENTS;
                
                if (expected_student < static_cast<int>(client_sockets.size())) {
                    std::cout << std::endl << "Timeout! Simulating count " << current_count 
                              << " for student " << expected_student << std::endl;
                    
                    broadcastCount(current_count, expected_student);
                    current_count++;
                    total_counts++;
                    last_count_time = now;
                    
                    displayProgress(current_count - 1, now);
                }
            }
            
            if (current_count > 0) {
                last_count_time = std::chrono::steady_clock::now();
            }
        }
    }
    
    void run() {
        std::thread accept_thread(&TCPCountingServer::acceptClients, this);
        std::thread timeout_thread(&TCPCountingServer::simulateTimeouts, this);
        
        std::string input;
        std::cout << "Press 'q' and Enter to quit..." << std::endl;
        while (std::getline(std::cin, input)) {
            if (input == "q" || input == "quit") {
                break;
            }
        }
        
        cleanup();
        
        if (accept_thread.joinable()) accept_thread.join();
        if (timeout_thread.joinable()) timeout_thread.join();
    }
    
    void cleanup() {
        running = false;
        
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
        
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int socket : client_sockets) {
            close(socket);
        }
        client_sockets.clear();
    }
};

int main() {
    TCPCountingServer server;
    
    if (!server.start()) {
        return 1;
    }
    
    server.run();
    
    std::cout << std::endl << "Server shutdown complete." << std::endl;
    return 0;
}