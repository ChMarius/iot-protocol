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
static struct sockaddr_in server;

/* WiFi state tracking */
static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static bool wifi_connected = false;

/* WiFi event callback */
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback dhcp_cb;

static K_SEM_DEFINE(dhcp_ready_sem, 0, 1);

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                                uint64_t mgmt_event,
                                struct net_if *iface)
{
    printk("wifi: event received 0x%llx\n", mgmt_event);

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        printk("wifi: connected!\n");
        wifi_connected = true;
        k_sem_give(&wifi_connected_sem);
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        printk("wifi: disconnected\n");
        wifi_connected = false;
    }
}

static void dhcp_event_handler(struct net_mgmt_event_callback *cb,
                                uint64_t mgmt_event,
                                struct net_if *iface)
{
    printk("dhcp: event received 0x%llx\n", mgmt_event);

    if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
        printk("dhcp: IP assigned, network ready\n");
        k_sem_give(&dhcp_ready_sem);
    }
}

static int wait_for_network(void)
{
    struct net_if *iface = net_if_get_default();
    int retries = 20;

    while (retries-- > 0) {
        /* Check interface is up AND has an IPv4 address */
        if (net_if_is_up(iface)) {
            struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
            if (ipv4 && ipv4->unicast[0].ipv4.is_used) {
                printk("network: interface ready\n");
                return 0;
            }
        }
        printk("network: waiting for interface... (%d)\n", retries);
        k_msleep(500);
    }

    printk("network: interface never became ready\n");
    return -1;
}


int wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        printk("wifi: no interface\n");
        return -1;
    }

    /* Separate callbacks for WiFi and DHCP events */
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                  NET_EVENT_WIFI_CONNECT_RESULT |
                                  NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(&dhcp_cb, dhcp_event_handler,
                                  NET_EVENT_IPV4_DHCP_BOUND);
    net_mgmt_add_event_callback(&dhcp_cb);

    net_dhcpv4_start(iface);
    printk("wifi: DHCP started\n");

    struct wifi_connect_req_params params = {
        .ssid        = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk         = WIFI_PSK,
        .psk_length  = strlen(WIFI_PSK),
        .channel     = WIFI_CHANNEL_ANY,
        .security    = WIFI_SECURITY_TYPE_PSK,
        .band        = WIFI_FREQ_BAND_2_4_GHZ,
        .mfp         = WIFI_MFP_DISABLE,
    };

    printk("wifi: connecting to %s...\n", WIFI_SSID);

    int retries = 3;
    int ret = -1;
    while (retries-- > 0) {
        ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
                    &params, sizeof(params));
        if (ret == 0) break;
        printk("wifi: connect attempt failed (%d), retrying...\n", ret);
        k_msleep(2000);
    }
    if (ret != 0) {
        printk("wifi: all connect attempts failed\n");
        return ret;
    }

    printk("wifi: waiting for WiFi...\n");
    if (k_sem_take(&wifi_connected_sem, K_SECONDS(30)) != 0) {
        printk("wifi: timeout\n");
        return -2;
    }

    printk("wifi: waiting for DHCP...\n");
    if (k_sem_take(&dhcp_ready_sem, K_SECONDS(30)) != 0) {
        printk("wifi: DHCP timeout\n");
        return -3;
    }

    printk("wifi: fully ready, IP: 192.168.x.x\n");
    return 0;
}

void init_server_addr(void)
{
    server.sin_family = AF_INET;
    server.sin_port   = htons(SERVER_PORT);

    if (zsock_inet_pton(AF_INET, SERVER_IP, &server.sin_addr) != 1) {
        printk("Invalid server IP\n");
    }
}

int prepare_socket(void)
{
    /* Small delay to let the network stack fully settle after DHCP */
    k_msleep(500);

    /* Open socket if not open, or if rotation interval reached */
    if (sock >= 0) {
        zsock_close(sock);
        printk("transport: socket closed for rotation\n");
    }

    if(wait_for_network() != 0) {
        printk("transport: network not ready, cannot open socket\n");
        return -1;
    }

    k_msleep(200); /* brief pause before reopening socket */

    sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    /* In Zephyr, valid fds start at 0 but -1 or negative means error */
    if (sock < 0) {
        printk("Failed to open socket (err %d, errno %d)\n", sock, errno);
        return -1;
    }

    printk("Socket opened (fd=%d)\n", sock);

    send_count = 0; /* reset send count for new socket */
    printk("transport: socket ready (fd=%d)\n", sock);
    return 0;
}
