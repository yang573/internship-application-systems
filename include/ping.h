#ifndef PING_H
#define PING_H

/*
typedef struct ip_header_t {
	// line 1
	uint8_t	 version_ihl;
	uint8_t  service_type;
	uint16_t length;
	// line 2
	uint16_t id;
	uint16_t flags_and_offset;
	// line 3
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t checksum;
	// line 4
	uint32_t src_ip;
	uint32_t dest_ip;
} ip_hdr;

typedef struct icmp_header_t {
	// line 1
	uint8_t	 msg_type;
	uint8_t  code;
	uint16_t checksum;
	// line 2
	uint16_t id;
	uint16_t seq_num;
} icmp_hdr;
*/

int send_ping(const char *node);

#endif
