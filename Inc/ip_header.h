#ifndef IP_HEADER_H
#define IP_HEADER_H

#include <stdint.h>
#include <stddef.h>

/* Simple and lightweight IP header for IoT protocol */
/* Total header size: 20 bytes (IPv4-like but simplified) */

#define IP_HEADER_VERSION 4
#define IP_HEADER_IHL 5          /* Internet Header Length in 32-bit words */
#define IP_HEADER_TOTAL_LEN 20   /* Total header length in bytes */

#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17
#define IP_PROTOCOL_CUSTOM 99    /* For our IoT protocol */

struct ip_header {
    uint8_t version_ihl;        /* Version (4 bits) + IHL (4 bits) */
    uint8_t tos;               /* Type of Service */
    uint16_t total_length;     /* Total length of IP packet */
    uint16_t identification;   /* Identification for fragmentation */
    uint16_t flags_offset;     /* Flags (3 bits) + Fragment offset (13 bits) */
    uint8_t ttl;              /* Time to Live */
    uint8_t protocol;          /* Protocol (TCP=6, UDP=17, Custom=99) */
    uint16_t checksum;         /* Header checksum */
    uint32_t src_ip;           /* Source IP address */
    uint32_t dst_ip;           /* Destination IP address */
};

/* Helper functions for IP header manipulation */
void ip_header_init(struct ip_header *header, uint32_t src_ip, uint32_t dst_ip, uint8_t protocol);
uint16_t ip_header_checksum(const struct ip_header *header);
void ip_header_set_total_length(struct ip_header *header, uint16_t length);
uint16_t ip_header_get_total_length(const struct ip_header *header);

#endif /* IP_HEADER_H */