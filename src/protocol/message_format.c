#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#define IOTP_HDR_LEN 6

static inline uint32_t iotp_serial_mask(uint32_t serial)
{
    return serial & 0x00FFFFFFu;
}

static inline int iotp_encode_header(uint8_t *buf, size_t buf_len, const struct iotp_header *hdr)
{
    if (!buf || !hdr) {
        return -EINVAL;
    }
    if (buf_len < IOTP_HDR_LEN) {
        return -ENOSPC;
    }

    buf[0] = hdr->device_type;
    buf[1] = hdr->protocol_version;
    buf[2] = hdr->command;
    sys_put_be24(iotp_serial_mask(hdr->device_serial), &buf[3]); /* Zephyr helper */ /* [2](https://docs.zephyrproject.org/latest/doxygen/html/sys_2byteorder_8h.html)[3](https://zephyr-docs.listenai.com/doxygen/html/byteorder_8h.html) */

    return IOTP_HDR_LEN;
}

static inline int iotp_decode_header(const uint8_t *buf, size_t len, struct iotp_header *out_hdr,
                     const uint8_t **out_payload, size_t *out_payload_len)
{
    if (!buf || !out_hdr) {
        return -EINVAL;
    }
    if (len < IOTP_HDR_LEN) {
        return -EMSGSIZE;
    }

    out_hdr->device_type = buf[0];
    out_hdr->protocol_version = buf[1];
    out_hdr->command = buf[2];
    out_hdr->device_serial = sys_get_be24(&buf[3]); /* Zephyr helper */ /* [2](https://docs.zephyrproject.org/latest/doxygen/html/sys_2byteorder_8h.html)[3](https://zephyr-docs.listenai.com/doxygen/html/byteorder_8h.html) */

    if (out_payload) {
        *out_payload = &buf[IOTP_HDR_LEN];
    }
    if (out_payload_len) {
        *out_payload_len = len - IOTP_HDR_LEN;
    }

    return 0;
}

/* Buffer builder helpers */

static inline int iotp_put_u8(uint8_t *buf, size_t buf_len, size_t *off, uint8_t v)
{
    if (!buf || !off) {
        return -EINVAL;
    }
    if (*off + 1 > buf_len) {
        return -ENOSPC;
    }
    buf[*off] = v;
    *off += 1;
    return 0;
}

static inline int iotp_put_mem(uint8_t *buf, size_t buf_len, size_t *off, const void *src, size_t n)
{
    if (!buf || !off || (!src && n)) {
        return -EINVAL;
    }
    if (*off + n > buf_len) {
        return -ENOSPC;
    }
    memcpy(&buf[*off], src, n);
    *off += n;
    return 0;
}

/*
 * Append a TLV.
 * Returns 0 on success, negative errno on failure.
 */
static inline int iotp_tlv_put_raw(uint8_t *buf, size_t buf_len, size_t *off,
                   uint8_t type, const void *value, uint8_t value_len)
{
    int rc;

    rc = iotp_put_u8(buf, buf_len, off, type);
    if (rc) {
        return rc;
    }
    rc = iotp_put_u8(buf, buf_len, off, value_len);
    if (rc) {
        return rc;
    }
    return iotp_put_mem(buf, buf_len, off, value, value_len);
}

static inline int iotp_tlv_put_u16_be(uint8_t *buf, size_t buf_len, size_t *off, uint8_t type, uint16_t v)
{
    uint8_t tmp[2];
    sys_put_be16(v, tmp); /* Zephyr helper */ /* [2](https://docs.zephyrproject.org/latest/doxygen/html/sys_2byteorder_8h.html)[3](https://zephyr-docs.listenai.com/doxygen/html/byteorder_8h.html) */
    return iotp_tlv_put_raw(buf, buf_len, off, type, tmp, (uint8_t)sizeof(tmp));
}

static inline int iotp_tlv_put_s16_be(uint8_t *buf, size_t buf_len, size_t *off, uint8_t type, int16_t v)
{
    return iotp_tlv_put_u16_be(buf, buf_len, off, type, (uint16_t)v);
}

static inline int iotp_tlv_put_u32_be(uint8_t *buf, size_t buf_len, size_t *off, uint8_t type, uint32_t v)
{
    uint8_t tmp[4];
    sys_put_be32(v, tmp); /* Zephyr helper */ /* [2](https://docs.zephyrproject.org/latest/doxygen/html/sys_2byteorder_8h.html)[3](https://zephyr-docs.listenai.com/doxygen/html/byteorder_8h.html) */
    return iotp_tlv_put_raw(buf, buf_len, off, type, tmp, (uint8_t)sizeof(tmp));
}

static inline int iotp_tlv_put_s32_be(uint8_t *buf, size_t buf_len, size_t *off, uint8_t type, int32_t v)
{
    return iotp_tlv_put_u32_be(buf, buf_len, off, type, (uint32_t)v);
}

/* TLV parser */

typedef int (*iotp_tlv_cb_t)(const struct iotp_tlv_view *tlv, void *user_data);

static inline int iotp_tlv_iterate(const uint8_t *payload, size_t payload_len, iotp_tlv_cb_t cb, void *user_data)
{
    size_t off = 0;

    if (!payload || !cb) {
        return -EINVAL;
    }

    while (off < payload_len) {
        struct iotp_tlv_view v;

        if (payload_len - off < 2) {
            return -EBADMSG;
        }

        v.type = payload[off + 0];
        v.len  = payload[off + 1];

        off += 2;

        if (payload_len - off < v.len) {
            return -EBADMSG;
        }

        v.value = &payload[off];

        off += v.len;

        int rc = cb(&v, user_data);
        if (rc) {
            return rc;
        }
    }

    return 0;
}

/* Convenience decoders for common numeric TLVs */

static inline int iotp_tlv_as_u16_be(const struct iotp_tlv_view *tlv, uint16_t *out)
{
    if (!tlv || !out) {
        return -EINVAL;
    }
    if (tlv->len != 2) {
        return -EMSGSIZE;
    }
    *out = sys_get_be16(tlv->value); /* Zephyr helper */ /* [2](https://docs.zephyrproject.org/latest/doxygen/html/sys_2byteorder_8h.html)[3](https://zephyr-docs.listenai.com/doxygen/html/byteorder_8h.html) */
    return 0;
}

static inline int iotp_tlv_as_s16_be(const struct iotp_tlv_view *tlv, int16_t *out)
{
    uint16_t tmp;
    int rc = iotp_tlv_as_u16_be(tlv, &tmp);
    if (rc) {
        return rc;
    }
    *out = (int16_t)tmp;
    return 0;
}

static inline int iotp_tlv_as_u32_be(const struct iotp_tlv_view *tlv, uint32_t *out)
{
    if (!tlv || !out) {
        return -EINVAL;
    }
    if (tlv->len != 4) {
        return -EMSGSIZE;
    }
    *out = sys_get_be32(tlv->value); /* Zephyr helper */ /* [2](https://docs.zephyrproject.org/latest/doxygen/html/sys_2byteorder_8h.html)[3](https://zephyr-docs.listenai.com/doxygen/html/byteorder_8h.html) */
    return 0;
}

static inline int iotp_tlv_as_s32_be(const struct iotp_tlv_view *tlv, int32_t *out)
{
    uint32_t tmp;
    int rc = iotp_tlv_as_u32_be(tlv, &tmp);
    if (rc) {
        return rc;
    }
    *out = (int32_t)tmp;
    return 0;
}