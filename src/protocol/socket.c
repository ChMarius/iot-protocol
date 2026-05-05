#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>


void prepare_socket(void)
{
    /* Small delay to let the network stack fully settle after DHCP */
    k_msleep(500);

    /* Open socket if not open, or if rotation interval reached */
    if (sock < 0 || send_count >= ROTATION_INTERVAL) {
        if (sock >= 0) {
            zsock_close(sock);
            printk("transport: port rotated after %d sends\n", send_count);
        }
    
        sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        /* In Zephyr, valid fds start at 0 but -1 or negative means error */
        if (sock < 0) {
            printk("Failed to open socket (err %d, errno %d)\n", sock, errno);
            return;
        }

        printk("Socket opened (fd=%d)\n", sock);

        struct zsock_timeval timeout = {
            .tv_sec  = 30,
            .tv_usec = 0
        };

        int ret = zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                                    &timeout, sizeof(timeout));
        if (ret < 0) {
            printk("Failed to set socket timeout (err %d)\n", ret);
        }
    }
}