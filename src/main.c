#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>

int main(void)
{
    usb_enable(NULL);
    k_msleep(2000);

    printk("Testing raw socket support...\n");

    int sock = zsock_socket(AF_INET, SOCK_RAW, 253);

    if (sock < 0) {
        printk("FAIL: raw socket not supported, errno=%d\n", errno);
    } else {
        printk("SUCCESS: raw socket opened, fd=%d\n", sock);
        zsock_close(sock);
    }

    while (1) {
        k_msleep(1000);
    }

    return 0;
}