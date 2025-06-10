#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <csignal>
#include <cstdlib>

class TCPCountingServer {
private:
    static const int PORT = 35701;
    int max_students;
    
    int server_fd;
    std::vector<int> client_sockets;
    std::mutex clients_mutex;
    std::atomic<int> current_count{0};
    std::atomic<bool> running{true};
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_display;
    std::chrono::steady_clock::time_point last_count_time; // Time of the last count
    std::chrono::steady_clock::time_point last_rate_reset; // Time of the last rate reset
    std::atomic<int> total_counts{0};
    std::atomic<int> counts_since_last_reset{0}; // Counts since last rate reset
    bool fast_mode = false;

public:
    TCPCountingServer(int students = 13) : server_fd(-1), max_students(students) {
        client_sockets.reserve(max_students);
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
        
        if (listen(server_fd, max_students) < 0) {
            perror("Listen failed");
            return false;
        }
        
        std::cout << "TCP Counting Server listening on port " << PORT << std::endl;
        std::cout << "Waiting for students to connect..." << std::endl;
        
        start_time = std::chrono::steady_clock::now();
        last_display = start_time;
        last_count_time = start_time;
        last_rate_reset = start_time;
        counts_since_last_reset = 0;
        
        return true;
    }
    
    void acceptClients() {
        while (running && client_sockets.size() < max_students) {
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
            
            std::cout << "Client connected. Total clients: " << client_sockets.size() 
                      << "/" << max_students << std::endl;
            
            std::thread(&TCPCountingServer::handleClient, this, client_socket).detach();
        }
    }
    
    void handleClient(int client_socket) {
        char buffer[1024];
        std::string partial_buffer;
        
        while (running) {
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
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
                    
                    // Simplified: Just accept all counts assuming clients collaborate nicely
                    current_count = received_count + 1;
                    total_counts++;
                    counts_since_last_reset++;
                    
                    auto now = std::chrono::steady_clock::now();
                    // Update the last count time to prevent timeout
                    last_count_time = now;
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_display);
                    
                    if (!fast_mode || elapsed.count() >= 500) {
                        displayProgress(received_count, now);
                        last_display = now;
                    }
                    
                    // Switch to fast mode if we're getting counts quickly (less than 100ms apart)
                    if (elapsed.count() < 100 && !fast_mode) {
                        fast_mode = true;
                        std::cout << "Switching to fast mode (displaying every 500ms)..." << std::endl;
                    }
                    
                    // Switch back to regular mode if we're getting counts slowly (more than 500ms apart)
                    if (elapsed.count() > 500 && fast_mode) {
                        fast_mode = false;
                        std::cout << "Switching back to regular mode..." << std::endl;
                    }
                    
                    // Still broadcast to keep all clients in sync
                    broadcastCount(received_count);
                } catch (const std::exception& e) {
                    // Ignore invalid lines
                }
            }
        }
        
        close(client_socket);
    }
    
    void displayProgress(int count, std::chrono::steady_clock::time_point now) {
        // Calculate total rate for reference
        auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double total_seconds = total_elapsed.count() / 1000.0;
        double total_rate = (total_seconds > 0) ? total_counts / total_seconds : 0;
        
        // Calculate current rate (since last reset)
        auto current_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rate_reset);
        double current_seconds = current_elapsed.count() / 1000.0;
        double current_rate = (current_seconds > 0) ? counts_since_last_reset / current_seconds : 0;
        
        // Reset the rate counter every 5 seconds to get a more current rate
        if (current_seconds >= 5.0) {
            last_rate_reset = now;
            counts_since_last_reset = 0;
        }
        
        if (fast_mode) {
            std::cout << "\rCurrent count: " << count 
                      << " | Rate: " << std::fixed << std::setprecision(1) << current_rate << " counts/sec    ";
            std::cout.flush();
        } else {
            std::cout << "Count " << count << " from student " << (count % max_students) 
                      << " (Rate: " << std::fixed << std::setprecision(1) << current_rate << " counts/sec)" << std::endl;
        }
    }
    
    void broadcastCount(int count) {
        std::string message = std::to_string(count) + "\n";
        
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (client_sockets.empty()) {
            // No clients to broadcast to, but we still need to update the state
            return;
        }
        
        for (int socket : client_sockets) {
            send(socket, message.c_str(), message.length(), 0);
        }
    }
    
    void simulateTimeouts() {
        bool started = false;
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            auto now = std::chrono::steady_clock::now();
            auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_count_time);
            
            // Wait 30 seconds before starting the simulation if no one connects
            if (!started && client_sockets.size() == 0) {
                auto since_start = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
                if (since_start.count() > 30) {
                    std::cout << "No students connected after 30 seconds. Starting simulation..." << std::endl;
                    started = true;
                }
            }
            
            // Simulate counts after a timeout (5 seconds)
            if ((since_last.count() > 5000 && client_sockets.size() > 0) || 
                (started && client_sockets.size() == 0 && since_last.count() > 5000)) {
                int expected_student = current_count % max_students;
                
                // Check if the expected student is connected
                bool student_connected = false;
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    student_connected = expected_student < static_cast<int>(client_sockets.size());
                }
                
                // Either the student is connected but timed out, or the student isn't connected at all
                std::cout << std::endl << "5-second timeout! Simulating count " << current_count 
                          << " for student " << expected_student 
                          << (student_connected ? " (connected but timed out)" : " (not connected)") 
                          << std::endl;
                
                // Broadcast the simulated count, then increment
                broadcastCount(current_count);
                current_count++;
                total_counts++;
                counts_since_last_reset++;
                last_count_time = now; // Update the shared last count time
                
                displayProgress(current_count - 1, now);
            }
            
            // No need to reset the timer here - we update it when we receive or simulate a count
        }
    }
    
    void run() {
        std::thread accept_thread(&TCPCountingServer::acceptClients, this);
        std::thread timeout_thread(&TCPCountingServer::simulateTimeouts, this);
        
        std::cout << "Server running. Press Ctrl-C to quit..." << std::endl;
        
        // Wait for threads to complete (they'll run until program termination)
        if (accept_thread.joinable()) accept_thread.join();
        if (timeout_thread.joinable()) timeout_thread.join();
        
        // Note: This point is only reached if a thread exits unexpectedly
        cleanup();
    }
    
public:
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

// Global pointer to server for signal handler
TCPCountingServer* g_server_ptr = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_server_ptr) {
        g_server_ptr->cleanup();
    }
    exit(signal);
}

int main(int argc, char* argv[]) {
    int students = 13;  // Default value
    
    if (argc > 1) {
        students = std::atoi(argv[1]);
        if (students <= 0) {
            std::cout << "Error: number of students must be positive" << std::endl;
            return 1;
        }
    }
    
    // Set up signal handler for Ctrl-C (SIGINT)
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "Starting TCP Counting Server with " << students << " students" << std::endl;
    TCPCountingServer server(students);
    g_server_ptr = &server;  // Set global pointer for signal handler
    
    if (!server.start()) {
        return 1;
    }
    
    server.run();
    
    std::cout << std::endl << "Server shutdown complete." << std::endl;
    return 0;
}