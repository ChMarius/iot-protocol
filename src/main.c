#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

int main(void)
{
    usb_enable(NULL);
    k_msleep(2000);

    while (1) {
        printk("Pico W heartbeat\n");
        k_msleep(1000);
    }

    return 0;
}