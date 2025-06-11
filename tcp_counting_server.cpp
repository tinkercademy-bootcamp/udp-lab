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
#include <algorithm>

class TCPCountingServer {
private:
    static const int PORT = 35701;
    int max_students;
    int max_connections;
    
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
    TCPCountingServer(int students = 13) : server_fd(-1), max_students(students), max_connections(students * 3) {
        client_sockets.reserve(max_connections);
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
        
        if (listen(server_fd, max_connections) < 0) {
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
        while (running && client_sockets.size() < max_connections) {
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
            
            std::cout << "\nClient connected. Total clients: " << client_sockets.size() 
                      << "/" << max_connections << std::endl;
            
            std::thread(&TCPCountingServer::handleClient, this, client_socket).detach();
        }
    }
    
    void handleClient(int client_socket) {
        char buffer[1024];
        std::string partial_buffer;
        
        while (running) {
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                // Client disconnected or error occurred
                removeClient(client_socket);
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
                    
                    // Still broadcast to keep all clients in sync
                    broadcastCount(received_count);
                } catch (const std::exception& e) {
                    // Ignore invalid lines
                }
            }
        }
        
        // Socket is closed in removeClient
    }
    
    void displayProgress(int count, std::chrono::steady_clock::time_point now, bool timeout = false) {
        // Calculate total rate for reference
        auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        double total_seconds = total_elapsed.count() / 1000.0;
        double total_rate = (total_seconds > 0) ? total_counts / total_seconds : 0;
        
        // Calculate current rate (since last reset)
        auto current_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rate_reset);
        double current_milliseconds = current_elapsed.count();
        double current_rate = (current_milliseconds > 0) ? counts_since_last_reset / current_milliseconds : 0;
        
        if (fast_mode) {
            std::cout << "\rCurrent count: " << count 
                      << " | Rate: " << std::fixed << std::setprecision(3) << current_rate << " counts/ms    ";
            std::cout.flush();
            // reset the rate counter on every display
            last_rate_reset = now;
            counts_since_last_reset = 0;
        } else {            
            std::cout << (timeout ? "\n[timeout] " : "\n") << "Count " << count << " from student " << (count % max_students);
            std::cout.flush();

            // reset the rate counter only after a full round of counts
            if (count % max_students == 0 && current_milliseconds > 2000) {
                last_rate_reset = now;
                counts_since_last_reset = 0;
            }
        }

        fast_mode = current_rate > 5.0;
    }
    
    void removeClient(int client_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = std::find(client_sockets.begin(), client_sockets.end(), client_socket);
        if (it != client_sockets.end()) {
            client_sockets.erase(it);
            close(client_socket);
            std::cout << "\nClient disconnected. Total clients: " << client_sockets.size() 
                      << std::endl;
        }
    }
    
    void broadcastCount(int count) {
        std::string message = std::to_string(count) + "\n";
        
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (client_sockets.empty()) {
            // No clients to broadcast to, but we still need to update the state
            return;
        }
        
        // Use iterator to safely remove failed sockets during broadcast
        auto it = client_sockets.begin();
        while (it != client_sockets.end()) {
            ssize_t result = send(*it, message.c_str(), message.length(), MSG_NOSIGNAL);
            if (result < 0) {
                // Send failed, client likely disconnected
                close(*it);
                it = client_sockets.erase(it);
                std::cout << "\nClient disconnected during broadcast. Total clients: " 
                          << client_sockets.size() << "/" << max_connections << std::endl;
            } else {
                ++it;
            }
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
                                
                // Broadcast the simulated count, then increment
                broadcastCount(current_count);
                current_count++;
                total_counts++;
                counts_since_last_reset++;
                last_count_time = now; // Update the shared last count time
                
                displayProgress(current_count - 1, now, true);
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
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <total_students>" << std::endl;
        return 1;
    }
    
    int students = std::atoi(argv[1]);
    if (students <= 0) {
        std::cout << "Error: number of students must be positive" << std::endl;
        return 1;
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