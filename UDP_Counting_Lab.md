# UDP Multicast Counting Lab
*Tinkercademy Bootcamp Summer 2025*

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
- Before starting, skim through this [Cloudflare primer on TCP vs UDP](https://www.cloudflare.com/en-gb/learning/ddos/glossary/user-datagram-protocol-udp/) and pay particular attention to the TCP Handshake diagram

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

## Phase 2: Multicast on the Command Line

### Objective
Experience multicast group communication within the VPC. Note that 239.255.1.1 is the only multicast group that is configured to work in your AWS VPC. Because AWS VPCs are not true local networks, we had to configure an AWS Transit Gateway to simulate the multicast support in a local network.

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
- Try and see whether any messages are dropped or received out of order (though it will probably be too rare to spot)

---

## Phase 3: TCP Counting Game

We'll play the counting game over TCP first, before getting around to UDP in Phase 4. This is inspired by discord counting, except it's truly dumb counting - only numbers allowed - don't expect 2^4 to come after 3*5.

### Exercise 3.1: TCP Counting Game via telnet

The instructor will compile and run `tcp_counting_server.cpp` and present this on the screen.
```bash
g++ -o tcp_counting_server tcp_counting_server.cpp
./tcp_counting_server 13 # for 13 students
```

Students should number off from 0 to find out their id.

Each student should connect to the counting server on port 35701 using telnet
```bash
telnet trainers-in.tnkr.be 35701
```

Each student should type in next number if it's their turn (i.e. the next number module total students is equal to their id).

### Exercise 3.2: TCP Counting Game in code

Now, for faster counting students should compile and run `tcp_counting_client.cpp`
```bash
g++ -o tcp_counting_client tcp_counting_client.cpp
./tcp_counting_client # see the usage instructions for providing id and total students
```

After all students have connected and the counts are increasing steadily, take note of how fast the numbers are increasing.

Discuss what the bottlenecks are.

---

## Phase 4: UDP Multicast Counting Game

Now we'll do the same in UDP

### Exercise 4.1 UDP Multicast Chat

Before getting to counting, let's try out the simple chat client in `udp_multicast_example.cpp`

```bash
g++ -o udp_multicast_example udp_multicast_example
./udp_multicast_example
```

### Exercise 4.2 Write your own UDP Counting Peer

The instructor will run `udp_counting_presenter.cpp` and keep it presenting on screen.
```bash
g++ -o udp_counting_presenter udp_counting_presenter.cpp
./udp_counting_presenter 13 # for 13 students
```

If time permits, the instructor will ask you to reference `udp_multicast_example.cpp` and write your own UDP Counting Peer.

Otherwise, you'll be asked to just run the sample
```bash
g++ -o udp_counting_peer udp_counting_peer.cpp
./udp_counting_peer # see the usage instructions for providing id and total students
```

See how fast it is.

---

## Debrief Questions

1. **Latency Comparison:** Which version (TCP vs UDP) was faster? Why?

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
