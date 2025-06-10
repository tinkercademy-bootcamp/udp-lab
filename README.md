# UDP Multicast Counting Lab

A hands-on lab for learning UDP multicast communication, designed for HFT developer bootcamp students.

## Quick Start

```bash
# Compile all programs
make all

# For students
./tcp_client <student_id>     # TCP version
./udp_skeleton <student_id>   # UDP version (skeleton to complete)

# For instructor
./tcp_server                  # TCP coordinator
./udp_presenter              # UDP monitor with timeout simulation
```

## Lab Structure

### Phase 1: Command-Line UDP Tools
- Use `nc` and `socat` for basic UDP communication
- Understand connectionless nature

### Phase 2: Multicast Discovery  
- Join multicast group `239.255.1.1:12345`
- Send/receive simple messages

### Phase 3: TCP Counting Game
- Centralized coordination via TCP server
- Measure baseline performance

### Phase 4: UDP Multicast Counting
- Distributed counting via multicast
- Experience reliability challenges

## Files

| File | Purpose |
|------|---------|
| `UDP_Counting_Lab.md` | Complete lab instructions |
| `tcp_counting_server.cpp` | Instructor TCP server |
| `tcp_counting_client.cpp` | Student TCP client |
| `udp_counting_skeleton.cpp` | Student UDP template |
| `udp_counting_model.cpp` | Complete UDP implementation |
| `udp_instructor_presenter.cpp` | Instructor UDP monitor |
| `instructor_notes.md` | Teaching guidance and troubleshooting |

## Prerequisites

```bash
# Install dependencies (Ubuntu)
make install-deps

# Verify environment
make verify-environment
```

## Compilation

```bash
# Build everything
make all

# Student programs only
make student

# Instructor programs only  
make instructor
```

## Running the Lab

### TCP Phase (Phase 3)
1. Instructor runs: `./tcp_server`
2. Students run: `./tcp_client <student_id>` (0-12)
3. Time how long to reach count 1000

### UDP Phase (Phase 4)  
1. Instructor runs: `./udp_presenter`
2. Students implement and run: `./udp_skeleton <student_id>`
3. Compare performance and reliability vs TCP

## Key Learning Outcomes

- **Performance**: UDP multicast is faster but less reliable
- **Trade-offs**: Latency vs reliability in distributed systems
- **Real-world**: Why HFT uses UDP for market data, TCP for orders
- **Challenges**: Packet loss, duplication, ordering issues

## Troubleshooting

See `instructor_notes.md` for detailed troubleshooting guide.

### Common Issues
- **Port conflicts**: Use `sudo netstat -ulnp | grep 12345`
- **Multicast routing**: Check `ip route show | grep 239`
- **Permissions**: Some commands may need `sudo`

### Network Monitoring
```bash
# Monitor multicast traffic
sudo tcpdump -i any host 239.255.1.1

# Check group membership
cat /proc/net/igmp
```

## Advanced Features

The model implementation (`udp_counting_model.cpp`) includes:
- Duplicate detection
- Gap analysis  
- Performance statistics
- Out-of-order handling

This serves as reference for students and demonstrates production-quality features.

---

*Part of HFT Developer Bootcamp - Network Programming Module*