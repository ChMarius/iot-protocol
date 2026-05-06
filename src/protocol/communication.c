#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>

void send_message(void)
{
    uint8_t buffer[MSG_MAX_PACKET_LEN];
    struct msg_packet pkt;

    struct msg_header header = {
        .device_type = 0x01,
        .device_id = 0x0001,
        .protocol_version = 0x01,
        .flag = 0x00,
        .command = IOTP_CMD_POST,
    };

    int rc;

    /* Initialize packet and write the fixed header */
    rc = msg_packet_init(&pkt,
                          buffer,
                          sizeof(buffer),
                          &header);
    if (rc != MSG_OK) {
        printk("Packet init failed (%d)\n", rc);
        return;
    }

    /* Add temperature TLV
     * Value is scaled by 10
     * Example: 15.5 C is sent as 155
     */
    rc = msg_add_tlv_u16(&pkt,
                          IOTP_TLV_TEMPERATURE,
                          155);
    if (rc != MSG_OK) {
        printk("Temperature TLV failed (%d)\n", rc);
        return;
    }

    /* Add humidity TLV
     * Value is scaled by 10
     * Example: 65.3 percent is sent as 653
     */
    rc = msg_add_tlv_u16(&pkt,
                          IOTP_TLV_HUMIDITY,
                          653);
    if (rc != MSG_OK) {
        printk("Humidity TLV failed (%d)\n", rc);
        return;
    }

    /* Send the constructed packet over UDP */
    int sent = zsock_sendto(sock,
                            pkt.buffer,
                            pkt.offset,
                            0,
                            (struct sockaddr *)&server,
                            sizeof(server));
    if (sent < 0) {
        printk("Send failed (err %d)\n", sent);
        zsock_close(sock);
        sock = -1;
        return;
    }

    send_count++;
    printk("Sent %d bytes\n", sent);
}

void receive_ack(void)
{
    char ack_buf[32] = {0};
    struct sockaddr_in from = {0};
    socklen_t from_len = sizeof(from);

    int received = zsock_recvfrom(sock,
                                  ack_buf, sizeof(ack_buf) - 1,
                                  0,
                                  (struct sockaddr *)&from,
                                  &from_len);
    if (received < 0) {
        printk("No ACK received (timeout or error %d)\n", received);
        return;
    }

    ack_buf[received] = '\0';
    printk("ACK received: %s\n", ack_buf);
}
