# UDP Counting Lab

I'm teaching a C++ bootcamp for aspiring high-frequency trading (HFT) developers. The students already have experience with low-level C++ and sockets. Please review the exercises in this GitHub repository (link provided) to understand their current level and capabilities. For more details on what they know, take a look at the exercises in the tinker-chat subdirectory.

Now, I want to design a lab project to introduce them to UDP multicast, with a focus on real-world reliability issues, order coordination, and HFT-relevant concepts.

The lab should progressively build understanding in four stages, culminating in a collaborative exercise.

## Lab Goal

Create a collaborative distributed program (inspired by Discord's counting game) where:

- Each student (13 total) is assigned a unique ID (0–12).
- They work together to count from 0 upward.
- Each program broadcasts the next number only if the number mod 13 equals its assigned ID.
- Communication should happen via UDP multicast.

## Lab Environment

- Each student already has access to a Ubuntu environment running in an AWS instance on the same VPC
- They have sudo access and can install anything not already included

## Lab Steps

The lab should proceed in the following phases:

1. Warm-up: Basic UDP with Command-Line Tools

- Use command-line tools (like nc, socat, netcat, or whatever else you recommend) to send and receive UDP messages.
- Let students open multiple terminals or SSH into multiple instances to simulate message passing.
- This builds intuition around UDP’s lack of connection or delivery guarantees.

2. Multicast with UDP

- Introduce a standard multicast IP/port for all students to join.
- Have each student send and receive simple UDP messages (e.g. hello from student 5) to the multicast group.
- Reinforce that multicast works within the same VPC subnet and discuss group management.

3. Counting Game with TCP

- Provide sample C++ code using TCP where each student connects to a central coordinator or server.
- They "take turns" counting up in a sequence, only speaking up when it's their turn.
- This version should work reliably, but introduce latency and bottlenecks due to centralized coordination.
- They can compare how long it takes the class to reach a large number (e.g., 1000).
- The code should have an option to show every single number, or if counts are cominng in quickly, only 
  display the current count and last number received every 500ms, with a summary of counts/second so that the
  terminal display lag doesn't affect the counting speed

4. Counting Game with UDP Multicast
- Share skeleton C++ code for sending/receiving multicast UDP datagrams.
- Each student implements their counting client to broadcast their number when appropriate.
- While waiting for others to finish their code, they can simulate classmates' messages using socat or prior tools.
- The skeleton code should have an option to show every single number, or if counts are cominng in quickly, only 
  display the current count and last number received every 500ms, with a summary of counts/second so that the
  terminal display lag doesn't affect the counting speed.

# Debrief & Reflection

At the end of the lab, include a guided discussion on:

- Benefits of UDP multicast: low latency, minimal coordination, parallel scalability.
- Drawbacks: packet loss, message duplication, race conditions, lack of delivery or ordering guarantees.
- Implications for HFT: why some data (e.g. market data) is multicast, while orders use TCP.

# Deliverables

Please generate:

- Lab instructions (markdown file) that include some introduction and theory and basic steps. For each
  step that invokes the command line, it should provide a description of the command line options used.
- Skeleton code snippets (C++)
- Notes for instructors: gotchas to watch for, how to simulate loss, how to verify student understanding
- Instructor/presentation code that will be running on the instructor's computer and projectec on screen:
  - Both TCP server code, and UDP node code that functions as a presenter
  - If no count is received after a 500ms timeout, just simulate the count on behalf of the student (so that
    the count can still proceed, albeit slowly, while students are preparing their code)
  - If counts are cominng in quickly, only display the current count and last number received every 500ms, with
    a summary of counts/second so that the terminal display lag doesn't affect the counting speed
- Model code for the UDP client