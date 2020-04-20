#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>

#include "defs.h"
#include "ping.h"

/* Derived from RFC 1071 memo
 */
static uint16_t icmp_checksum(void *addr, int length)
{
	unsigned short *a = addr;
	unsigned long sum = 0;

	for (; length > 1; length -= 2)
	{
		sum += *a++;
	}

	if (length > 0)
		sum += *((unsigned char*)addr);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return (uint16_t)(~sum);
}

int send_ping(const char *node)
{
	struct addrinfo hints, *list, *addr;
	int socket_fd;
	int ret;

	//struct ip ip;
	struct icmp icmp_pkt;
	struct sockaddr_in dst_addr;//, ret_addr;

	/* Get address info for socket */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;
	hints.ai_flags = AI_PASSIVE;

	ret = getaddrinfo(node, NULL, &hints, &list);
	if (ret != 0)
	{
		const char *error_msg = gai_strerror(ret);
		fprintf(stderr, "%s: %s\n", node, error_msg);
		return ret;
	}

	/* Connect to socket */
	for (addr = list; addr != NULL; addr = addr->ai_next)
	{
		socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (socket_fd == -1)
			perror("socket:");
		else
		{
			//ip.ip_dst = addr->ai_addr->sin_addr;

			dst_addr.sin_addr = ((struct sockaddr_in*)(addr->ai_addr))->sin_addr;
			break;
		}
	}

	freeaddrinfo(list);

	if (socket_fd == -1)
		return -1;

	/* Setup socket */
	// Set the default TTL field for all packets
	// TODO: Error checking
	ret = setsockopt(socket_fd, IPPROTO_IP, IP_TTL, &((int){DEFAULT_TTL}), sizeof(int));

	// Enable retrieval of TTL field
	ret = setsockopt(socket_fd, IPPROTO_IP, IP_RECVTTL, &((int){1}), sizeof(int));

	/* Setup packet */
	/*
	// TODO: Check EVERY SINGLE field
	ip.ip_hl = 5;
	ip.ip_v = 4;
	ip.ip_tos = 0;
	ip.ip_len = htons(sizeof(ip));
	ip.ip_id = 0;
	ip.ip_off = 0x40;
	ip.ip_ttl = DEFAULT_TTL; 
	ip.p = IPPROTO_ICMP;
	ip.sum;

	ip.ip_src = 0;
	*/

	// TODO: Check EVERY SINGLE field
	memset(&icmp_pkt, 0, sizeof(icmp_pkt));
	icmp_pkt.icmp_type = ICMP_ECHO;
	icmp_pkt.icmp_code = 0;
	icmp_pkt.icmp_cksum = 0;
	icmp_pkt.icmp_id = 13;
	icmp_pkt.icmp_seq = 1;

	icmp_pkt.icmp_cksum = icmp_checksum(&icmp_pkt, sizeof(icmp_pkt));

	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = IPPROTO_ICMP;

	//unsigned int ret_addr_length = sizeof(ret_addr);

	/* Sending */
	ret = sendto(socket_fd, &icmp_pkt, sizeof(icmp_pkt), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));
	if (ret == -1)
	{
		perror("sendto");
	}

	/* Receiving */
	//ret = select();

	struct msghdr msg;
	char control_buf[256];
	char iov_buf[256];
	char cmsg_buf[512];

	struct iovec msg_iov;
	msg_iov.iov_base = iov_buf;
	msg_iov.iov_len = sizeof(iov_buf);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &msg_iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_buf;
	msg.msg_controllen = sizeof(control_buf);
	msg.msg_flags = 0;

	ret = recvmsg(socket_fd, &msg, 0);
	//ret = recvfrom(socket_fd, buf, sizeof(buf), 0, (struct sockaddr*)&ret_addr, &ret_addr_length);
	if (ret == -1)
	{
		perror("recvfrom");
	}

	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
		fprintf(stderr, "cmsg level: %d type: %d len: %ld\n",
				cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
		memcpy(&cmsg_buf, CMSG_DATA(cmsg), cmsg->cmsg_len);
		cmsg_buf[cmsg->cmsg_len] = '\0';
		fprintf(stderr, "cmsg data: %s\n", cmsg_buf);
	}

	//fprintf(stderr, "bytes: %d icmp_type: %d\n", ret, icmp_resp->icmp_type);
	fprintf(stderr, "bytes: %d\n", ret);

	close(socket_fd);

	return 0;
}

int recv_pkt()
{
	/* Receiving */
	//ret = select();
	//ret = recvfrom();
	return 0;
}

