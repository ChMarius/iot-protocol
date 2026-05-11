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

static uint32_t tx_interval_ms = DEFAULT_TX_INTERVAL_MS;


int main(void)
{
    usb_enable(NULL);
    k_msleep(2000);

    printk("\n=== IoT Client Starting ===\n");

    /* Connect to WiFi first */
    if (wifi_connect() != 0) {
        printk("main: WiFi connection failed, halting\n");
        return -1;
    }

    k_msleep(4000);
    
    init_server_addr();
    while (1)
    {
        send_message();

        printk("main: sleeping for %u ms before next send\n",
            tx_interval_ms);

        k_msleep(tx_interval_ms);
   }
    
    return 0;
}