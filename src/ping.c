#include <pthread.h>
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
#include "cbuf.h"

struct sockaddr_in dst_addr;
int ping_loop = 1;
pthread_mutex_t ping_loop_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ping_loop_cond = PTHREAD_COND_INITIALIZER;

static struct cbuf pkt_buf;
static pthread_mutex_t pkt_buf_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pkt_buf_cond = PTHREAD_COND_INITIALIZER;

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

int init_ping(const char *node, int ttl)
{
	struct addrinfo hints, *list, *addr;
	int socket_fd;
	int pkg_buf_len;
	int ret;

	if (ttl < 1 || ttl > 255)
	{
		fprintf(stderr, "Invalid TTL value\n");
		return ERR_USAGE;
	}

	/* Get address info for socket */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;
	hints.ai_flags = 0;

	ret = getaddrinfo(node, NULL, &hints, &list);
	if (ret != 0)
	{
		const char *error_msg = gai_strerror(ret);
		fprintf(stderr, "Error on %s: %s\n", node, error_msg);
		return ERR_FATAL;
	}

	/* Connect to socket */
	for (addr = list; addr != NULL; addr = addr->ai_next)
	{
		socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (socket_fd == -1)
		{
			perror("socket:");
		}
		else
		{
			dst_addr.sin_addr = ((struct sockaddr_in*)(addr->ai_addr))->sin_addr;
			break;
		}
	}

	freeaddrinfo(list);

	if (socket_fd == -1)
		return ERR_FATAL;

	/* Setup socket */
	// Set the default TTL field for all packets
	ret = setsockopt(socket_fd, IPPROTO_IP, IP_TTL, &((int){DEFAULT_TTL}), sizeof(int));
	if (ret == -1)
	{
		perror("Error setting up socket");
		return ERR_FATAL;
	}

	// Enable retrieval of TTL field
	ret = setsockopt(socket_fd, IPPROTO_IP, IP_RECVTTL, &((int){1}), sizeof(int));
	if (ret == -1)
	{
		perror("Error setting up socket");
		return ERR_FATAL;
	}

	pkg_buf_len = ttl / PING_DELAY;
	pkg_buf_len += pkg_buf_len / 10;

	init_cbuf(&pkt_buf, ttl / pkg_buf_len);

	return socket_fd;
}

int cleanup_ping(int socket_fd)
{
	int error = 0;
	int ret;

	ret = delete_cbuf(&pkt_buf);
	if (ret != 0)
	{
		fprintf(stderr, "Error cleaning up buffers\n");
		error = 1;
	}

	close(socket_fd);
	if (ret != 0)
	{
		perror("Error closing socket");
		error = 1;
	}

	return error;
}

void* ping_routine(void* arg)
{
	int socket_fd = (int)(intptr_t)arg;
	struct sockaddr_in dst_addr;
	struct icmp icmp_pkt;
	int seq = 0;
	int ret;

	// Setup destination address
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = IPPROTO_ICMP;

	// TODO: Check EVERY SINGLE field
	memset(&icmp_pkt, 0, sizeof(icmp_pkt));
	icmp_pkt.icmp_type = ICMP_ECHO;
	icmp_pkt.icmp_code = 0;
	icmp_pkt.icmp_cksum = 0;
	icmp_pkt.icmp_id = 13;

	while (ping_loop)
	{
		icmp_pkt.icmp_seq = seq;
		seq++;
		icmp_pkt.icmp_cksum = icmp_checksum(&icmp_pkt, sizeof(icmp_pkt));

		pthread_mutex_lock(&pkt_buf_lock);
		ret = sendto(socket_fd, &icmp_pkt, sizeof(icmp_pkt), 0,
				(struct sockaddr*)&dst_addr, sizeof(dst_addr));
		if (ret == -1)
		{
			perror("Error sending packets");
			pthread_mutex_unlock(&ping_loop_lock);
			// TODO: Stop other thread and clean up
			return (void*)(intptr_t)ERR_FATAL;
		}
		//cbuf_push(&pkt_buf, seq-1);
		pthread_cond_signal(&pkt_buf_cond);
		pthread_mutex_unlock(&pkt_buf_lock);

		sleep(1);
	}

	pthread_mutex_lock(&pkt_buf_lock);
	pthread_cond_signal(&pkt_buf_cond);
	pthread_mutex_unlock(&pkt_buf_lock);

	fprintf(stderr, "Exiting ping routine\n");
	return 0;
}

void* recv_routine(void* arg)
{
	int socket_fd = (int)(intptr_t)arg;
	struct msghdr msg;
	struct iovec msg_iov;
	char control_buf[256];
	char iov_buf[256];
	char cmsg_buf[512];
	int ret;

	msg_iov.iov_base = iov_buf;
	msg_iov.iov_len = sizeof(iov_buf);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &msg_iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_buf;
	msg.msg_controllen = sizeof(control_buf);
	msg.msg_flags = 0;

	while (ping_loop)
	{
		pthread_mutex_lock(&pkt_buf_lock);
		if (cbuf_empty(&pkt_buf))
			pthread_cond_wait(&pkt_buf_cond, &pkt_buf_lock);
		pthread_mutex_unlock(&pkt_buf_lock);

		ret = recvmsg(socket_fd, &msg, 0);
		if (ret == -1)
		{
			perror("Error reading packet");
			// TODO: Stop other thread and clean up
			return (void*)(intptr_t)ERR_FATAL;
		}


		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
				cmsg != NULL;
				cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
			fprintf(stderr, "cmsg level: %d type: %d len: %ld\n",
					cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
			memcpy(&cmsg_buf, CMSG_DATA(cmsg), cmsg->cmsg_len);
			cmsg_buf[cmsg->cmsg_len] = '\0';
			fprintf(stderr, "cmsg data: %s\n", cmsg_buf);
		}
	}

	fprintf(stderr, "Exiting recv routine\n");
	return 0;
}

int ping_results(void)
{
	return 0;
}

