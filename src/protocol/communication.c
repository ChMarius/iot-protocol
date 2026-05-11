#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "communication.h"
#include "IP_connect.h"

static int sock = -1;
static int send_count = 0;
static int tx_interval_ms = DEFAULT_TX_INTERVAL_MS;
static struct sockaddr_in server;


void send_message(void)
{
    const char *msg = "Hello from Pico W!";
    
    if (sock < 0 || send_count >= ROTATION_INTERVAL) {
        if (prepare_socket() < 0) {
            printk("transport: cannot open socket, skipping send");
            return;
        }
    }

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

    receive_ack(); /* wait for ACK before counting this send */

    send_count++;
    printk("Sent %d bytes, send_count=%d/%d\n", sent, send_count, ROTATION_INTERVAL);
}

static void handle_server_command(const char *cmd)
{
    if (cmd == NULL) {
        return;
    }

    /*
     * Expected command format:
     * SET_INTERVAL_MS:<number>
     *
     * Example:
     * SET_INTERVAL_MS:10000
     */
    if (strncmp(cmd, "SET_INTERVAL_MS:", 16) == 0) {
        const char *value_str = cmd + 16;
        char *endptr = NULL;

        long new_interval = strtol(value_str, &endptr, 10);

        /*
         * Validate that:
         * 1. The command actually contained a number.
         * 2. The number is inside the allowed range.
         */
        if (endptr == value_str) {
            printk("command: invalid interval value\n");
            return;
        }

        if (new_interval < MIN_TX_INTERVAL_MS ||
            new_interval > MAX_TX_INTERVAL_MS) {
            printk("command: interval out of range (%ld ms)\n", new_interval);
            return;
        }

        tx_interval_ms = (uint32_t)new_interval;

        printk("command: transmission interval changed to %u ms\n",
               tx_interval_ms);

        return;
    }

    /*
     * Placeholder commands for exam demo.
     * These do not need hardware behaviour yet.
     */
    if (strcmp(cmd, "PING") == 0) {
        printk("command: PING received\n");
        return;
    }

    if (strcmp(cmd, "LED_ON") == 0) {
        printk("command: LED_ON received\n");
        return;
    }

    if (strcmp(cmd, "LED_OFF") == 0) {
        printk("command: LED_OFF received\n");
        return;
    }

    printk("command: unknown command: %s\n", cmd);
}

static void process_ack_message(const char *ack)
{
    const char *cmd_prefix = "ACK|CMD=";
    size_t prefix_len = strlen(cmd_prefix);

    if (ack == NULL) {
        return;
    }

    /*
     * Plain ACK.
     * No command attached.
     */
    if (strcmp(ack, "ACK") == 0) {
        printk("ack: plain ACK received\n");
        return;
    }

    /*
     * ACK with command.
     * Example:
     * ACK|CMD=SET_INTERVAL_MS:10000
     */
    if (strncmp(ack, cmd_prefix, prefix_len) == 0) {
        const char *cmd = ack + prefix_len;

        printk("ack: command attached: %s\n", cmd);
        handle_server_command(cmd);
        return;
    }

    printk("ack: unknown ACK format: %s\n", ack);
}


void receive_ack(void)
{
    char ack_buf[32] = {0};
    struct sockaddr_in from = {0};
    socklen_t from_len = sizeof(from);
    struct zsock_pollfd fds[1];
    fds[0].fd     = sock;
    fds[0].events = ZSOCK_POLLIN;

    int ret = zsock_poll(fds, 1, 10000);

    if (ret < 0) {
        printk("transport: poll error (%d)\n", ret);
        zsock_close(sock);
        sock = -1;
        return;
    }

    if (ret == 0) {
        printk("transport: ACK timeout\n");
        zsock_close(sock);
        sock = -1;
        return;
    }

    if (fds[0].revents & ZSOCK_POLLIN) {
        struct sockaddr_in from = {0};
        socklen_t from_len = sizeof(from);

        int received = zsock_recvfrom(sock,
                                      ack_buf, sizeof(ack_buf) - 1,
                                      0,
                                      (struct sockaddr *)&from,
                                      &from_len);
        if (received < 0) {
            printk("transport: recvfrom error (%d)\n", received);
            zsock_close(sock);
            sock = -1;
            return;
        }

        
        ack_buf[received] = '\0';

        printk("ACK received: %s\n", ack_buf);

        /* Check whether the ACK also contains a server command */
        process_ack_message(ack_buf);
    }
}