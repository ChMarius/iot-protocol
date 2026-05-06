#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "Inc/message_format.h"

int msg_build_header(uint8_t *buf,
                     size_t buf_len,
                     const struct msg_header *header)
{
    if (!buf || !header) {
        return MSG_ERR_INVALID;
    }

    if (buf_len < MSG_HEADER_LEN) {
        return MSG_ERR_NO_SPACE;
    }

    buf[0] = header->device_type;

    buf[1] = (uint8_t)(header->device_id >> 8);
    buf[2] = (uint8_t)(header->device_id & 0xFF);

    buf[3] = header->protocol_version;
    buf[4] = header->flag;
    buf[5] = header->command;

    return MSG_HEADER_LEN;
}

int msg_add_tlv(uint8_t *buf,
                size_t buf_len,
                size_t *offset,
                uint8_t type,
                const void *value,
                uint8_t value_len)
{
    if (!buf || !offset || !value) {
        return MSG_ERR_INVALID;
    }

    if ((*offset + MSG_TLV_HEADER_LEN + value_len) > buf_len) {
        return MSG_ERR_NO_SPACE;
    }

    buf[*offset] = type;
    *offset += 1;

    buf[*offset] = value_len;
    *offset += 1;

    for (uint8_t i = 0; i < value_len; i++) {
        buf[*offset + i] = ((const uint8_t *)value)[i];
    }

    *offset += value_len;

    return MSG_OK;
}

int msg_packet_init(struct msg_packet *pkt,
                    uint8_t *buffer,
                    size_t buffer_len,
                    const struct msg_header *header)
{
    if (!pkt || !buffer || !header) {
        return MSG_ERR_INVALID;
    }

    pkt->buffer = buffer;
    pkt->buffer_len = buffer_len;
    pkt->offset = 0;

    int hdr_len = msg_build_header(pkt->buffer,
                                   pkt->buffer_len,
                                   header);
    if (hdr_len < 0) {
        return hdr_len;
    }

    pkt->offset = (size_t)hdr_len;

    return MSG_OK;
}