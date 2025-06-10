CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pthread
INCLUDES = -I.

# Targets
TCP_SERVER = tcp_server
TCP_CLIENT = tcp_client
UDP_SKELETON = udp_skeleton
UDP_MODEL = udp_model
UDP_PRESENTER = udp_presenter

# Source files
TCP_SERVER_SRC = tcp_counting_server.cpp
TCP_CLIENT_SRC = tcp_counting_client.cpp
UDP_SKELETON_SRC = udp_counting_skeleton.cpp
UDP_MODEL_SRC = udp_counting_model.cpp
UDP_PRESENTER_SRC = udp_instructor_presenter.cpp

# Default target
all: $(TCP_SERVER) $(TCP_CLIENT) $(UDP_SKELETON) $(UDP_MODEL) $(UDP_PRESENTER)

# TCP targets
$(TCP_SERVER): $(TCP_SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

$(TCP_CLIENT): $(TCP_CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

# UDP targets
$(UDP_SKELETON): $(UDP_SKELETON_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

$(UDP_MODEL): $(UDP_MODEL_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

$(UDP_PRESENTER): $(UDP_PRESENTER_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

# Individual builds for students
tcp: $(TCP_SERVER) $(TCP_CLIENT)
	@echo "TCP programs built successfully"
	@echo "Run instructor server: ./$(TCP_SERVER)"
	@echo "Run student client: ./$(TCP_CLIENT) <student_id>"

udp: $(UDP_SKELETON) $(UDP_MODEL) $(UDP_PRESENTER)
	@echo "UDP programs built successfully"
	@echo "Run instructor presenter: ./$(UDP_PRESENTER)"
	@echo "Run student skeleton: ./$(UDP_SKELETON) <student_id>"
	@echo "Run model implementation: ./$(UDP_MODEL) <student_id>"

# Student-only targets (simplified for lab)
student: tcp_client udp_skeleton
	@echo "Student programs built successfully"

instructor: tcp_server udp_presenter udp_model
	@echo "Instructor programs built successfully"

# Clean up
clean:
	rm -f $(TCP_SERVER) $(TCP_CLIENT) $(UDP_SKELETON) $(UDP_MODEL) $(UDP_PRESENTER)
	@echo "All binaries cleaned"

# Test compilation without running
test-compile: all
	@echo "All programs compiled successfully - ready for lab"

# Quick test of basic functionality
test-basic:
	@echo "Testing basic compilation..."
	@$(MAKE) clean
	@$(MAKE) all
	@echo "✓ All programs compile successfully"

# Install required packages (for Ubuntu)
install-deps:
	@echo "Installing required packages..."
	sudo apt update
	sudo apt install -y build-essential netcat-openbsd socat tcpdump net-tools
	@echo "✓ Dependencies installed"

# Lab setup verification
verify-environment:
	@echo "Verifying lab environment..."
	@echo -n "C++ compiler: "; which g++ || echo "❌ g++ not found"
	@echo -n "Netcat: "; which nc || echo "❌ nc not found"
	@echo -n "Socat: "; which socat || echo "❌ socat not found"
	@echo -n "Tcpdump: "; which tcpdump || echo "❌ tcpdump not found"
	@echo "Multicast route check:"
	@ip route show | grep 224 || echo "No multicast routes found"
	@echo "Environment verification complete"

# Help target
help:
	@echo "UDP Counting Lab Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build all programs"
	@echo "  tcp           - Build TCP server and client"
	@echo "  udp           - Build UDP skeleton, model, and presenter"
	@echo "  student       - Build only student programs"
	@echo "  instructor    - Build only instructor programs"
	@echo "  clean         - Remove all binaries"
	@echo "  test-compile  - Test that everything compiles"
	@echo "  install-deps  - Install required Ubuntu packages"
	@echo "  verify-env    - Check lab environment setup"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Usage for students:"
	@echo "  make student"
	@echo "  ./tcp_client <student_id>"
	@echo "  ./udp_skeleton <student_id>"
	@echo ""
	@echo "Usage for instructor:"
	@echo "  make instructor"
	@echo "  ./tcp_server"
	@echo "  ./udp_presenter"

.PHONY: all tcp udp student instructor clean test-compile test-basic install-deps verify-environment help