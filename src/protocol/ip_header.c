#include <zephyr/kernel.h>
#include <string.h>

#include "Inc/ip_header.h"

/* Initialize IP header with basic values */
void ip_header_init(struct ip_header *header, uint32_t src_ip, uint32_t dst_ip, uint8_t protocol)
{
    if (!header) {
        return;
    }

    memset(header, 0, sizeof(struct ip_header));

    /* Set version (4) and IHL (5) in first byte */
    header->version_ihl = (IP_HEADER_VERSION << 4) | IP_HEADER_IHL;

    /* Set default values */
    header->tos = 0;
    header->total_length = IP_HEADER_TOTAL_LEN;  /* Will be updated when payload is added */
    header->identification = 0;  /* Can be set to a sequence number */
    header->flags_offset = 0;    /* No fragmentation */
    header->ttl = 64;            /* Default TTL */
    header->protocol = protocol;
    header->checksum = 0;        /* Will be calculated later */
    header->src_ip = src_ip;
    header->dst_ip = dst_ip;

    /* Calculate checksum */
    header->checksum = ip_header_checksum(header);
}

/* Calculate IP header checksum (simple implementation) */
uint16_t ip_header_checksum(const struct ip_header *header)
{
    if (!header) {
        return 0;
    }

    uint32_t sum = 0;
    const uint16_t *data = (const uint16_t *)header;

    /* Sum all 16-bit words in header */
    for (size_t i = 0; i < sizeof(struct ip_header) / 2; i++) {
        sum += data[i];
    }

    /* Add carry bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    /* One's complement */
    return (uint16_t)~sum;
}

/* Set total length of IP packet */
void ip_header_set_total_length(struct ip_header *header, uint16_t length)
{
    if (!header) {
        return;
    }

    header->total_length = length;

    /* Recalculate checksum since total_length changed */
    header->checksum = 0;
    header->checksum = ip_header_checksum(header);
}

/* Get total length of IP packet */
uint16_t ip_header_get_total_length(const struct ip_header *header)
{
    return header ? header->total_length : 0;
}