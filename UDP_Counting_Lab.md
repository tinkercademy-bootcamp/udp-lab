# UDP Multicast Counting Lab
*HFT Developer Bootcamp - Network Programming Module*

## Overview

This lab introduces UDP multicast communication through a collaborative counting exercise. Students will progress from basic UDP concepts to implementing a distributed counting game that simulates real-world HFT market data scenarios.

## Learning Objectives

- Understand UDP vs TCP trade-offs in high-frequency systems
- Learn multicast group communication principles
- Experience packet loss, ordering, and timing issues firsthand
- Apply knowledge to HFT-relevant scenarios (market data feeds)

## Lab Environment

- Ubuntu AWS instances on shared VPC subnet
- Your AWS Security Group is configured to allow UDP on ports 36xxx
- Students have sudo access for package installation
- Each student assigned unique ID (0-12)
- Shared multicast group: `239.255.1.1:36702`

---

## Phase 1: UDP Fundamentals with Command-Line Tools

### Objective
Build intuition about UDP's connectionless nature using standard tools.

### Setup
Check and install required tools:
```bash
# Install socat
sudo apt update
sudo apt install socat
```

### Exercise 1.1: Basic UDP Send/Receive on localhost

**Terminal 1 (Receiver):**
```bash
socat UDP4-RECV:36701 -
```
- `UDP4-RECV:36701`: Listen for UDP packets on port 36701
- `-`: Send messages to STDOUT
- Should display each message as it arrives
- Stays alive after each sender completes

**Terminal 2 (Sender):**
```bash
echo "Message 1" | socat - UDP-DATAGRAM:localhost:36701
echo "Message 2" | socat - UDP-DATAGRAM:localhost:36701
echo "Message 3" | socat - UDP-DATAGRAM:localhost:36701
```
- `-`: Read data from STDIN (the echo pipe)
- `UDP-DATAGRAM:localhost:36701`: Send UDP packet to localhost port 36701
- Each command sends one packet and exits cleanly

**Expected Result:**
- Terminal 1 shows: "Message 1", "Message 2", "Message 3"
- Receiver stays alive and ready for more
- Don't worry about "W address is opened in read-write mode but only supports write-only" - it's just a SOCAT quirk
- If you get "Connection refused", the receiver isn't running!

### Exercise 1.2: UDP Across Different Hosts

Pairing up with the person next to you, find out your partner's hostname and send the UDP packet to your partner's AWS instance instead. Make sure your partner is still running their Receiver socat command.

**Sender Example:**
```bash
echo "Hello from Ayush" | socat - UDP-DATAGRAM:balaji.tnkr.be:36701
```

If you can install socat on your own laptop, you can also try to send the UDP message straight from your laptop. However, there's a high chance your hotel network might block UDP.

### Exercise 1.3: UDP "Fire and Forget"

If you send a message to a non-existent IP address, or to a port that is closed, socat won't give you an error because it has no idea that an error has occurred — UDP is a fire-and-forget protocol that sends messages without establishing a connection, guaranteeing delivery, or tracking whether the recipient receives them.

You'll see an error if you send to an unknown hostname however — this is raised from the DNS resolution attempt and not from UDP.

```bash
# Send to non-existent receiver
echo "Lost message" | socat - UDP-DATAGRAM:192.168.1.99:9999
```
- `192.168.1.99:9999`: Non-existent IP and port
- Notice: No error returned, packet is silently dropped
- This demonstrates UDP's "fire and forget" nature

---

## Phase 2: Multicast Discovery

### Objective
Experience multicast group communication within the VPC.

### Exercise 2.1: Join Multicast Group

**All students run (different terminals):**
```bash
socat UDP4-RECV:36702,ip-add-membership=239.255.1.1:0.0.0.0,reuseaddr -
```
- `UDP4-RECV:36702`: Listen on UDP port 36702
- `ip-add-membership=239.255.1.1:0.0.0.0`: Join multicast group 239.255.1.1 on any interface
- `reuseaddr`: Allow multiple processes to bind to same port (required for multicast)
- `-`: Output received data to STDOUT

### Exercise 2.2: Send to Multicast Group

**From any terminal:**
```bash
echo "Hello from student 5" | socat - UDP4-DATAGRAM:239.255.1.1:36702
```
- `-`: Read data from standard input (the echo pipe)
- `UDP4-DATAGRAM:239.255.1.1:36702`: Send UDP datagram to multicast address and port
- **Note:** All students listening should receive this message simultaneously

**Observe:** All students should receive the message simultaneously.

### Exercise 2.3: Timing Experiment
Students send a bunch of timestamped messages:
```bash
for i in {1..10}; do
   echo "$(date '+%H:%M:%S.%3N') - Student X" | socat - UDP4-DATAGRAM:239.255.1.1:36702
done
```
- `$(date '+%H:%M:%S.%3N')`: Current timestamp with milliseconds
- Replace "Student X" with your actual student ID

---

## Phase 3: TCP Counting Game (Baseline)

### Objective
Implement reliable centralized counting for performance comparison.

### Server Code (Instructor runs)
The instructor will run a TCP server that coordinates turns.

### Student Implementation
Each student compiles and runs the TCP client:

```bash
g++ -o tcp_counter tcp_counting_client.cpp
./tcp_counter <student_id>
```

Where `<student_id>` is your assigned number (0-12).

### Expected Behavior
- Students connect to central server
- Server coordinates turns in round-robin fashion
- Only the student whose turn it is can send the next number
- Count progresses: 0, 1, 2, 3, ... with student_id = count % 13

### Performance Measurement
Time how long it takes to reach count = 1000.

---

## Phase 4: UDP Multicast Counting Game

### Objective
Implement distributed counting with multicast UDP.

### Compilation
```bash
g++ -o udp_counter udp_counting_client.cpp
./udp_counter <student_id>
```

### Protocol Rules
1. Listen on multicast group `239.255.1.1:36702`
2. Send next number only when `current_count % 13 == your_student_id`
3. Message format: `"COUNT:<number>"`
4. Start with count = 0

### Expected Challenges
- Race conditions when multiple students send simultaneously
- Packet loss causing stuck counts
- Messages arriving out of order
- No central coordination

### Simulation During Development
While other students finish coding, simulate their messages:
```bash
# Simulate student 7 sending count 7
echo "COUNT:7" | socat STDIN UDP4-DATAGRAM:239.255.1.1:36702

# Simulate rapid counting
for i in {0..25}; do
  echo "COUNT:$i" | socat STDIN UDP4-DATAGRAM:239.255.1.1:36702
  sleep 0.1
done
```
- `COUNT:X`: Message format expected by the counting program
- `sleep 0.1`: Pause 100ms between messages to simulate realistic timing

### Performance Comparison
- Time to reach count = 1000 compared to TCP version
- Observe counts/second rate
- Note any stuck or skipped numbers

---

## Troubleshooting Commands

### Check Multicast Route
```bash
ip route show | grep 239
```

### Monitor Network Traffic
```bash
sudo tcpdump -i any host 239.255.1.1
```
- `-i any`: Listen on all interfaces
- `host 239.255.1.1`: Filter multicast traffic

### Test Multicast Connectivity
```bash
ping 239.255.1.1
```
Note: May not work depending on system configuration.

---

## Debrief Questions

1. **Latency Comparison:** Which version (TCP vs UDP) reached 1000 faster? Why?

2. **Reliability Issues:** What problems did you observe with UDP multicast?
   - Lost packets?
   - Duplicate messages?
   - Out-of-order delivery?

3. **Race Conditions:** What happened when multiple students tried to send the same number?

4. **HFT Applications:** 
   - Why might market data feeds use UDP multicast?
   - When would you still need TCP in trading systems?
   - How do exchanges handle the reliability issues you observed?

5. **Scalability:** How would each approach perform with 100 participants instead of 13?

---

## Real-World Context

### Market Data Feeds
- Exchanges broadcast price updates via UDP multicast
- Low latency critical (microseconds matter)
- Some packet loss acceptable for stale data
- Sequence numbers used to detect gaps

### Order Management
- Order submissions use TCP for reliability
- Confirmations must be guaranteed
- Higher latency acceptable for correctness

### Recovery Mechanisms
- Gap detection and retransmission requests
- Redundant feeds from multiple sources
- Local sequence number validation

This lab simulates these concepts at a manageable scale, giving you hands-on experience with the fundamental trade-offs in HFT system design.