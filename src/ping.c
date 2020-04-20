#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
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

static unsigned int n_pings_sent = 0;
static unsigned int n_pings_recv = 0;

struct pkt_seq {
	int seq;
	struct timeval time;
};

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

/*
static int cbuf_index(struct cbuf *buf, int seq)
{
	if (buf == NULL)
		return BUF_FATAL;
	if (buf->size == 0)
		return BUF_EMPTY;

	for (int i = buf->start; i < buf->end; ++i)
	{
		if ( ((struct pkt*)buf[i])->seq == seq)
			return i;
	}

	return -1;
}

static int cbuf_remove(struct cbuf *buf, int seq, struct pkt_seq **time_seq)
{
	if (buf == NULL)
		return BUF_FATAL;
	if (buf->size == 0)
		return BUF_EMPTY;

	int index = cbuf_index(buf, seq);
	if (index == -1)
		return -1;

	*time_seq = buf->buf[index];

	buf->start++;
	buf->size--;

	if (buf->start == buf->len)
		buf->start = 0;

	return 0;
}
*/

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

	init_cbuf(&pkt_buf, pkg_buf_len);

	return socket_fd;
}

int cleanup_ping(int socket_fd)
{
	int i;
	int error = 0;
	int ret;

	for (i = pkt_buf.start; i < pkt_buf.end; ++i)
		free(pkt_buf.buf[i]);

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
	struct pkt_seq *time_seq;
	struct timeval time;
	int seq = 0;
	int ret;

	// Setup destination address
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = IPPROTO_ICMP;

	memset(&icmp_pkt, 0, sizeof(icmp_pkt));
	icmp_pkt.icmp_type = ICMP_ECHO;
	icmp_pkt.icmp_code = 0;
	icmp_pkt.icmp_cksum = 0;
	icmp_pkt.icmp_id = getpid();

	while (ping_loop)
	{
		icmp_pkt.icmp_seq = seq;
		seq++;
		icmp_pkt.icmp_cksum = icmp_checksum(&icmp_pkt, sizeof(icmp_pkt));

		pthread_mutex_lock(&pkt_buf_lock);
		ret = gettimeofday(&time, NULL);
		if (ret == -1)
		{
			perror("Error getting time");
			goto ping_routine_error;
		}

		ret = sendto(socket_fd, &icmp_pkt, sizeof(icmp_pkt), 0,
				(struct sockaddr*)&dst_addr, sizeof(dst_addr));
		if (ret == -1)
		{
			perror("Error sending packets");
			goto ping_routine_error;
		}
		n_pings_sent++;

		time_seq = malloc(sizeof(struct pkt_seq));
		if (time_seq == NULL)
		{
			perror("Error tracking packet");
			goto ping_routine_error;
		}
		time_seq->seq = seq;
		time_seq->time = time;

		ret = cbuf_push(&pkt_buf, time_seq);
		if (ret != 0)
		{
			fprintf(stderr, "Error tracking packet: ");
			if (ret == BUF_NOSPACE)
				fprintf(stderr, "No space in circular buffer\n");
			else if (ret == BUF_FATAL)
				fprintf(stderr, "Circular buffer not initialized\n");
			free(time_seq);
			goto ping_routine_error;
		}

		pthread_cond_signal(&pkt_buf_cond);
		pthread_mutex_unlock(&pkt_buf_lock);

		sleep(1);
	}

	pthread_mutex_lock(&pkt_buf_lock);
	pthread_cond_signal(&pkt_buf_cond);
	pthread_mutex_unlock(&pkt_buf_lock);

	return 0;

ping_routine_error:
	pthread_mutex_unlock(&ping_loop_lock);

	pthread_mutex_lock(&ping_loop_lock);
	ping_loop = 0;
	pthread_cond_signal(&ping_loop_cond);
	pthread_mutex_unlock(&ping_loop_lock);

	return (void*)(intptr_t)ERR_FATAL;
}

void* recv_routine(void* arg)
{
	int socket_fd = (int)(intptr_t)arg;
	struct msghdr msg;
	struct iovec msg_iov;
	char control_buf[256];
	char iov_buf[256];
	//char cmsg_buf[512];
	int pkt_bytes;

	struct pkt_seq *time_seq;
	struct timeval time;
	long time_dif;
	//int seq;
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

		if (!ping_loop)
			break;

		pkt_bytes = recvmsg(socket_fd, &msg, 0);
		if (pkt_bytes == -1)
		{
			perror("Error reading packet");
			goto recv_routine_error;
		}
		n_pings_recv++;

		ret = gettimeofday(&time, NULL);
		if (ret == -1)
		{
			perror("Error getting time");
			goto recv_routine_error;
		}

		fprintf(stdout, "%d bytes ", pkt_bytes);

		pthread_mutex_lock(&pkt_buf_lock);
		//ret = cbuf_remove(&pkt_buf, seq, &time_seq)
		//if (ret == -1)
		ret = cbuf_pop(&pkt_buf, (void**)&time_seq);
		pthread_mutex_unlock(&pkt_buf_lock);

		time_dif = time.tv_sec - time_seq->time.tv_sec;
		fprintf(stdout, "%ld s ", time_dif);

		time_dif = time.tv_usec - time_seq->time.tv_usec;

		fprintf(stdout, "%ld us\n", time_dif);

		free(time_seq);

		/*
		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
				cmsg != NULL;
				cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
			fprintf(stderr, "cmsg level: %d type: %d len: %ld\n",
					cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
			memcpy(&cmsg_buf, CMSG_DATA(cmsg), cmsg->cmsg_len);
			cmsg_buf[cmsg->cmsg_len] = '\0';
			if (cmsg->cmsg_level == IPPROTO_IP)
			{
				if (cmsg->cmsg_type == IP_TOS)
					fprintf(stderr, "cmsg tos: %d\n", (int)cmsg_buf[0]);
				if (cmsg->cmsg_type == IP_TTL)
					fprintf(stderr, "cmsg ttl: %d\n", (int)cmsg_buf[0]);
			}
			else
				fprintf(stderr, "cmsg data: %s\n", cmsg_buf);
		}
		*/
	}

	return 0;

recv_routine_error:
	pthread_mutex_lock(&ping_loop_lock);
	ping_loop = 0;
	pthread_cond_signal(&ping_loop_cond);
	pthread_mutex_unlock(&ping_loop_lock);

	return (void*)(intptr_t)ERR_FATAL;
}

int ping_results(void)
{
	fprintf(stdout, "%d packets sents, %d packets received\n", n_pings_sent, n_pings_recv);

	return 0;
}

