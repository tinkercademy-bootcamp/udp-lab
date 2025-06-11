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
### Exercise 1.2: UDP Across Different Hosts
### Exercise 1.3: UDP "Fire and Forget"
### Exercise 2.1: Join Multicast Group
### Exercise 2.2: Send to Multicast Group
### Exercise 2.3: Timing Experiment
### Exercise 3.1: TCP Counting Game via telnet
### Exercise 3.2: TCP Counting Game in code
### Exercise 4.1 UDP Multicast Chat
### Exercise 4.2 Write your own UDP Counting Peer

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
