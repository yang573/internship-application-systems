YAPU - Yet Another Ping Utility
-------

This is by no means a complete ping utility, but it can send packets.

YAPU is written entirely in C for Linux. It employs the socket and (BSD-style) netinet libraries.

This program was developed and tested on a Ubuntu Desktop 18.04.03 LTS.

This program uses RAW sockets, which require root permissions.

* Building
```
make
```

* Running
```
sudo ./yapu ADDRESS
```

* Issues
- The ICMP packets being sent and/or received are malformed.
	- recvfrom(2) will return an icmp packet with an undocumented type (type 69)
	- recvmsg(2) will return a TTL of 64 and nothing else.

* Notes
- Due to the lack of useful information from the packets, RTT is calculated using gettimeofday(2) and storing the results
- The received ICMP packets do not contain sequence info that was originally provided, making it impossible to distinguish packets
	- We assume that any packets received correspond to the least recent packet
- The biggest difficulties with this project came from a lack of good documentation
	- Documentation for netinet/ip\_icmp.h consisted mostly of header file sources
	- Almost no documentation exists for constructing ICMP packets along with the correct fields
- fping, liboping, etc. sources proved helpful but cumbersome
- If I had to redo this project, I would just learn Gopher and implement ping with that

