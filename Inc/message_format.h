#ifndef MESSAGE_FORMAT_H
#define MESSAGE_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#define MSG_HDR_LEN 6
#define MSG_TLV_HDR_LEN 2
#define MSG_MAX_PACKET_LEN 64

enum msg_commands {
    MSG_CMD_POST = 0x01,
    MSG_CMD_GET  = 0x02,
    MSG_CMD_ACK  = 0x03,
};

enum msg_tlv_type {
    MSG_TLV_TEMPERATURE = 0x01, /* int16, value = Celsius * 10 */
    MSG_TLV_HUMIDITY    = 0x02, /* uint16, value = Percent * 10 */
    MSG_TLV_ERROR       = 0x7F,
};

/*
 * Fixed header, 6 bytes on wire:
 * byte 0: device_type
 * byte 1: device_id high byte
 * byte 2: device_id low byte
 * byte 3: protocol_version
 * byte 4: flag
 * byte 5: command
 */
struct msg_header {
    uint8_t device_type;
    uint16_t device_id;
    uint8_t protocol_version;
    uint8_t flag;
    uint8_t command;
};

/*
 * TLV item on wire:
 * byte 0: type
 * byte 1: length
 * byte 2..: value bytes
 */
struct msg_tlv_view {
    uint8_t type;
    uint8_t len;
    const uint8_t *value;
};

/* Function declarations */
int msg_encode_header(uint8_t *buf, size_t buf_len, const struct msg_header *header);

int msg_add_tlv_u16(uint8_t *buf,
                    size_t buf_len,
                    size_t *offset,
                    uint8_t type,
                    uint16_t value);

#endif