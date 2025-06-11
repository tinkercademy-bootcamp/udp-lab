# UDP Counting Lab - Instructor Notes

### Compilation Commands
Seems to work fine without -lpthread actually.

```bash
# TCP version
g++ -o tcp_server tcp_counting_server.cpp -lpthread
g++ -o tcp_client tcp_counting_client.cpp -lpthread

# UDP version  
g++ -o udp_skeleton udp_counting_skeleton.cpp -lpthread
g++ -o udp_model udp_counting_model.cpp -lpthread
g++ -o udp_presenter udp_instructor_presenter.cpp -lpthread
```

### AWS Setup (Already Done for Tinkercademy Bootcamp Summer 2025)
Apparently AWS VPCs don't support multicast by default. To get it working, we had to
- Create a VPC Transit Gateway with multicast enabled
- Create a Multicast Domain within the Gateway, and choose IGMP mode
- Create a VPC Transit Gateway Attachment on the VPC and relevant subnets
- Associate the Multicast Domain with the Attachment Point and VPC subnets
- Create a Group within the Multicast Domain and add all the Network Interfaces to it

---
## Issues and Discussion Points

### Exercise 1.1: Basic UDP Send/Receive on localhost

**Key Teaching Moments:**
- **Connectionless Nature**: Emphasize that receiver stays alive after sender exits - no connection to "close"
- **Port Binding**: Receiver must be running first (unlike TCP where connect() will wait)
- **One-Way Communication**: Sender has no feedback about delivery success
- **Process Management**: Show how UDP processes are independent - killing sender doesn't affect receiver

**Common Student Mistakes:**
- Starting sender before receiver → "Connection refused" error
- Expecting bidirectional communication without setting up reverse path
- Confusion about why receiver doesn't exit after first message

**Discussion Questions:**
- "What happens if you start the sender first?" (Connection refused)
- "How does this differ from a phone call vs leaving a voicemail?"
- "Can you have multiple senders from many different places?"
- "In HFT, would you want your market data receiver to 'hang up' after each price update?"

### Exercise 1.2: UDP Across Different Hosts

**Key Teaching Moments:**
- **Network Transparency**: UDP works the same locally and across networks
- **Firewall Reality**: Hotel/corporate networks often block UDP (prepare students for real-world limitations)
- **Hostname Resolution**: DNS works the same for UDP as TCP

**Common Student Mistakes:**
- Assuming partner's receiver is running
- Typos in hostnames
- Trying from restrictive networks (hotel WiFi)

**Discussion Questions:**
- "Why might a hotel network block UDP but allow TCP?"
- "What does this teach us about UDP's 'fire and forget' nature?"
- "In production HFT systems, how would you ensure network connectivity?"

### Exercise 1.3: UDP "Fire and Forget"

**Key Teaching Moments:**
- **Silent Failures**: UDP doesn't report delivery failures
- **Contrast with TCP**: TCP would give immediate connection errors
- **DNS vs UDP Errors**: DNS failures occur before UDP attempts
- **Production Implications**: Applications must handle uncertainty

**Common Student Mistakes:**
- Expecting error messages for failed deliveries
- Confusion about why DNS errors still occur

**Discussion Questions:**
- "How would a trading system detect if market data stopped arriving?"
- "What are the trade-offs of not getting delivery confirmations?"
- "When might 'fire and forget' be preferable to guaranteed delivery?"

### Exercise 2.1: Join Multicast Group

**Key Teaching Moments:**
- **Group Membership**: Multicast is opt-in, not automatic
- **reuseaddr Necessity**: Multiple processes must share the same port if runnin on the same machine
- **AWS Transit Gateway**: Touch on the special AWS setup required for multicast

**Common Student Mistakes:**
- Forgetting reuseaddr → "Address already in use" errors
- Starting multiple instances without understanding port sharing
- Assuming multicast works everywhere (it doesn't in most cloud environments)

**Discussion Questions:**
- "Why does reuseaddr work for UDP but not typically for TCP?"
- "What challenges would exchanges face deploying multicast in the cloud?"
- "How many trading firms could join a single multicast group?"

### Exercise 2.2: Send to Multicast Group

**Key Teaching Moments:**
- **Simultaneous Delivery**: All group members receive messages at once
- **No Unicast Overhead**: Single send reaches multiple recipients
- **Bandwidth Efficiency**: Compare to sending individual messages
- **Sender Doesn't Join**: Sender doesn't need group membership to transmit

**Common Student Mistakes:**
- Expecting sender to receive their own messages
- Not understanding why everyone gets the same message

**Discussion Questions:**
- "How does this compare to email with multiple recipients?"
- "What bandwidth savings does multicast provide for 1000 subscribers?"
- "Why don't exchanges need to establish 1000 TCP connections for market data?"

### Exercise 2.3: Timing Experiment

**Key Teaching Moments:**
- **Microsecond Timestamps**: Real HFT cares about microseconds, not milliseconds
- **Network Variability**: Even on AWS, timing varies
- **Race Conditions**: Multiple senders can create interesting ordering
- **Clock Synchronization**: Different machines have different clocks

**Common Student Mistakes:**
- Not replacing "Student X" with their actual ID

**Discussion Questions:**
- "What would happen if 13 students sent messages simultaneously?"
- "How do real trading systems ensure accurate timestamps?"
- "What timing precision do you think NYSE requires?"

### Exercise 3.1: TCP Counting Game via telnet

**Key Teaching Moments:**
- **Central Coordination**: Server enforces turn-taking
- **Connection State**: Each student has a persistent TCP connection
- **Blocking I/O**: Students wait for their turn (demonstrate with deliberate delay)
- **Reliability**: TCP guarantees message delivery and ordering

**Common Student Mistakes:**
- Sending numbers out of turn
- Confusion about connection persistence
- Impatience with waiting for turns

**Discussion Questions:**
- "What happens if a student's connection drops mid-game?"
- "How does the server enforce fairness?"
- "What are the scalability limits of this approach?"

### Exercise 3.2: TCP Counting Game in code

**Key Teaching Moments:**
- **Automation Speed**: Code is faster than humans but still limited by coordination
- **Network RTT**: Each count requires server round-trip
- **Serialization**: Only one student can count at a time
- **Resource Usage**: Server must maintain 13 TCP connections for 13 students

**Common Student Mistakes:**
- Not understanding why counting speed is limited
- Expecting linear speedup with more students

**Discussion Questions:**
- "What's the theoretical maximum counting rate?"
- "How does RTT affect performance in HFT?"
- "Why can't we just add more servers to go faster?"

### Exercise 4.1 UDP Multicast Chat

**Key Teaching Moments:**
- **No Central Server**: Pure peer-to-peer communication
- **Message Broadcasting**: Everyone sees everyone's messages
- **No Ordering Guarantees**: Messages may arrive out of sequence
- **Potential Duplication**: UDP doesn't prevent duplicate delivery

**Common Student Mistakes:**
- Expecting server-like coordination
- Surprise at seeing own messages (if sender joins group)

**Discussion Questions:**
- "How does this compare to TCP chat servers?"
- "What problems might arise with 1000 participants?"
- "How would you detect if someone stopped participating?"

### Exercise 4.2 Write your own UDP Counting Peer

**Key Teaching Moments:**
- **Race Conditions**: Multiple students may send the same number
- **Speed vs Reliability**: Much faster than TCP but less predictable
- **Self-Coordination**: No central authority to enforce rules
- **Error Recovery**: How to handle when counts get out of sync

**Common Student Mistakes:**
- Not handling race conditions gracefully
- Assuming perfect message delivery
- Creating infinite loops on conflicts

**Discussion Questions:**
- "What happens when two students send number 1000 simultaneously?"
- "How could we detect and recover from missed counts?"
- "What would real market data systems do about these problems?"
- "Is the speed improvement worth the complexity?"

**Final Synthesis Questions:**
- "When would you choose UDP vs TCP in a trading system?"
- "How do real exchanges handle the reliability issues we observed?"
- "What did this teach you about distributed systems challenges?"

---

## Advanced Simulation Techniques

Not for this round. Perhaps another time.

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
