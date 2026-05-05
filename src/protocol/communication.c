#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>

void send_message(void)
{
    const char *msg = "Hello from Pico W!";

    int sent = zsock_sendto(sock,
                            msg, strlen(msg),
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
    printk("Sent %d bytes\n", sent, send_count, ROTATION_INTERVAL);
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
