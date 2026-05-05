#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>

#include "Inc/IP_connect.h"
#include "Inc/communication.h"
#include "Inc/socket.h"

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
    
    init_server_addr();
    while(1)
    {
       prepare_socket();
       send_message();
       receive_ack();
       k_msleep(5000);
    }
    
    return 0;
}