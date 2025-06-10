# UDP Counting Lab - Instructor Notes

## Pre-Lab Setup

### Environment Verification
```bash
# Test multicast routing on instructor machine
ip route show | grep 239
ping -c 3 239.255.1.1

# Verify students can reach multicast group
echo "TEST" | socat - UDP4-DATAGRAM:239.255.1.1:12345
```

### Compilation Commands
Provide students with these exact commands:

```bash
# TCP version
g++ -o tcp_server tcp_counting_server.cpp -lpthread
g++ -o tcp_client tcp_counting_client.cpp -lpthread

# UDP version  
g++ -o udp_skeleton udp_counting_skeleton.cpp -lpthread
g++ -o udp_model udp_counting_model.cpp -lpthread
g++ -o udp_presenter udp_instructor_presenter.cpp -lpthread
```

---

## Phase 1: Command-Line Tools - Common Issues

**Issue: Permission denied on ports < 1024**
- **Problem:** Ports < 1024 are privileged and need sudo
- **Workaround:** Use ports > 1024 for exercises

**Issue: Multiple receivers - only one gets message**
- **Teaching Point:** This demonstrates UDP's unicast nature vs. multicast

**Issue: "Address already in use" when restarting receiver**
- **Problem:** OS keeps port in TIME_WAIT state briefly
- **Solution:** Wait 5-10 seconds or use different port numbers
- **Teaching moment:** Even UDP has some connection state in the OS

**Issue: Multiple receivers on same port**
- **Expected:** Only first `nc -u -l -p 8889` succeeds, others fail
- **Teaching point:** This is why we need multicast for multiple receivers

---

## Phase 2: Multicast Discovery - Critical Points

### Multicast Group Selection
- `239.255.1.1` is administratively scoped (local network only)
- Avoids conflicts with well-known multicast addresses

### AWS Setup
Apparently AWS VPCs don't support multicast by default. To get it working, we had to
- Create a VPC Transit Gateway with multicast enabled
- Create a Multicast Domain within the Gateway, and choose IGMP mode
- Create a VPC Transit Gateway Attachment on the VPC and relevant subnets
- Associate the Multicast Domain with the Attachment Point and VPC subnets
- Create a Group within the Multicast Domain and add all the Network Interfaces to it

### Common Socat Issues

**Issue: "socat: E address already in use"**
```bash
# Check what's using the port
sudo netstat -ulnp | grep 12345
# Kill conflicting processes
sudo pkill -f "12345"
```

**Discussion Points:**
- Are messages received in order?
- What happens if two students send simultaneously?

**Issue: Messages not received by all students**
- Check VPC security groups allow UDP multicast
- Verify subnet configuration supports multicast
- Test with: `tcpdump -i any host 239.255.1.1`

### Expected Observations
- All students receive same message simultaneously
- No delivery guarantee (some students might miss messages)
- Order may vary if network reorders packets

---

## Phase 3: TCP Counting - Instructor Presentation

### Running the TCP Server
```bash
./tcp_server
# Students connect with: ./tcp_client <student_id>
```

### Demonstration Points
1. **Sequential Nature:** Count progresses in perfect order
2. **Latency:** Each message requires round-trip to server
3. **Bottleneck:** Server becomes performance limitation
4. **Reliability:** No lost counts, no duplicates

### Timing the Exercise
- Start timer when first student connects
- Stop when count reaches 1000
- Typical results: 30-60 seconds for 1000 counts

### Troubleshooting TCP Phase
**Students can't connect:**
- Check server IP in client code (change from 127.0.0.1 to instructor's IP)
- Verify AWS security groups allow port 12346

**Server crashes:**
- Usually due to student disconnecting ungracefully
- Restart server, students reconnect

---

## Phase 4: UDP Multicast - The Real Challenge

### Running the Presenter
```bash
./udp_presenter
```
- Automatically simulates timeouts
- Displays real-time statistics
- Use 'reset' command to restart counting

### Expected Student Struggles

#### 1. Race Conditions
**Symptom:** Multiple students send same count
**Demonstration:** 
```bash
# Simulate race condition
echo "COUNT:42" | socat - UDP4-DATAGRAM:239.255.1.1:12345 &
echo "COUNT:42" | socat - UDP4-DATAGRAM:239.255.1.1:12345 &
```
**Teaching Point:** No coordination mechanism in UDP multicast

#### 2. Packet Loss
**Simulation:**
```bash
# Use tc (traffic control) to simulate 5% packet loss
sudo tc qdisc add dev eth0 root netem loss 5%
# Remove after demonstration:
sudo tc qdisc del dev eth0 root
```

#### 3. Out-of-Order Delivery
**Demonstration:** Send counts rapidly to show reordering
```bash
for i in {10..15}; do echo "COUNT:$i" | socat - UDP4-DATAGRAM:239.255.1.1:12345; done
```

### Performance Comparison
- UDP typically 5-10x faster than TCP version
- But with reliability issues (gaps, duplicates, stuck counts)

---

## Debugging Commands for Students

### Network Traffic Analysis
```bash
# Monitor all multicast traffic
sudo tcpdump -i any host 239.255.1.1 -n

# Show packet details
sudo tcpdump -i any host 239.255.1.1 -n -v

# Save to file for analysis
sudo tcpdump -i any host 239.255.1.1 -w multicast.pcap
```

### Multicast Group Membership
```bash
# Check joined groups
cat /proc/net/igmp

# Test multicast route
ip route get 239.255.1.1
```

### Process Monitoring
```bash
# See which students are running programs
ps aux | grep udp_

# Network connections
sudo netstat -ulnp | grep 12345
```

---

## Common Student Code Issues

### 1. Forgetting to Join Multicast Group
**Symptom:** Can send but not receive
**Fix:** Check `IP_ADD_MEMBERSHIP` setsockopt call

### 2. Wrong Bind Address
**Symptom:** "Address already in use"
**Common mistake:** Binding to multicast address instead of INADDR_ANY
**Fix:** 
```cpp
bind_addr.sin_addr.s_addr = INADDR_ANY;  // NOT inet_addr("239.255.1.1")
```

### 3. Buffer Overflow
**Symptom:** Segmentation fault
**Fix:** Always null-terminate received strings:
```cpp
buffer[bytes_received] = '\0';
```

### 4. Busy Waiting
**Symptom:** High CPU usage
**Fix:** Add sleep in counting loop:
```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(50));
```

---

## Advanced Simulation Techniques

### Simulating Network Issues

#### Packet Loss
```bash
# 10% random packet loss
sudo tc qdisc add dev eth0 root netem loss 10%
```

#### Latency Variation
```bash
# Add 100ms ± 20ms delay
sudo tc qdisc add dev eth0 root netem delay 100ms 20ms
```

#### Packet Duplication
```bash
# 5% packet duplication
sudo tc qdisc add dev eth0 root netem duplicate 5%
```

#### Remove All Rules
```bash
sudo tc qdisc del dev eth0 root
```

### Controlled Chaos Testing
1. Start all students with UDP clients
2. Apply network impairments gradually
3. Observe how counting degrades
4. Remove impairments and watch recovery

---

## Discussion Guide Questions

### Performance Analysis
1. "Which version reached count 1000 faster? Why?"
2. "What was the counts/second rate for each approach?"
3. "How would latency affect each approach?"

### Reliability Discussion  
1. "What reliability issues did you observe with UDP?"
2. "How many duplicate counts did you see?"
3. "Were there any gaps in the sequence?"
4. "What happened when multiple students tried to send the same count?"

### Real-World Applications
1. "Why might exchanges use UDP for market data?"
2. "When would you still need TCP in trading systems?"
3. "How do real systems handle the reliability issues we observed?"

### Scaling Considerations
1. "How would each approach perform with 100 participants?"
2. "What bottlenecks would emerge?"
3. "How could we improve the UDP approach?"

---

## Troubleshooting Student Environment Issues

### AWS VPC Configuration
- Ensure security groups allow UDP traffic on port 12345
- Verify instances are in same subnet/VPC
- Check route tables for multicast routing

### Ubuntu Package Issues
```bash
# Update package list
sudo apt update

# Install missing tools
sudo apt install tcpdump net-tools socat netcat-openbsd

# Check installed versions
socat -V
nc -h
```

### Firewall Issues
```bash
# Check if ufw is blocking traffic
sudo ufw status

# Allow multicast port (if needed)
sudo ufw allow 12345/udp
```

---

## Timing and Pacing

### Recommended Schedule (3-hour lab)
- **0:00-0:30:** Phase 1 (Command-line tools)
- **0:30-1:00:** Phase 2 (Multicast discovery)  
- **1:00-1:30:** Phase 3 (TCP implementation)
- **1:30-2:30:** Phase 4 (UDP implementation)
- **2:30-3:00:** Debrief and discussion

### Keeping Students Engaged
- While some students debug code, others can simulate their messages
- Use presenter screen to show class progress in real-time
- Encourage students to help each other with compilation issues
- Run mini-competitions (first to reach count X)

### Signs Students Are Struggling
- More than 15 minutes on compilation errors
- No one sending any UDP messages after 30 minutes
- Students not observing multicast messages from others

**Intervention Strategies:**
- Provide pre-compiled binaries
- Demo the compilation process on projector
- Use model implementation for students who fall behind

---

## Learning Assessment

### Formative Assessment (during lab)
- Can students explain why UDP is faster?
- Do they understand the trade-offs?
- Can they identify reliability issues they observe?

### Summative Assessment Questions
1. "Design a hybrid approach that combines TCP reliability with UDP performance"
2. "How would you detect and handle packet loss in the UDP version?"
3. "Explain why market data feeds use UDP multicast despite reliability issues"

### Success Criteria
Students should demonstrate understanding of:
- ✅ UDP vs TCP performance characteristics
- ✅ Multicast communication patterns  
- ✅ Packet loss and ordering issues
- ✅ Real-world applications in HFT systems