#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define WIFI_SSID    "UK-18"
#define WIFI_PSK     "z6r467Mg"

#define SERVER_IP    "192.168.1.243"
#define SERVER_PORT  5000

#define ROTATION_INTERVAL 5 /* rotate ports every n sends*/
#define ACK_TIMEOUT_SEC    10


#define DEFAULT_TX_INTERVAL_MS 5000
#define MIN_TX_INTERVAL_MS     1000
#define MAX_TX_INTERVAL_MS     60000

static uint32_t tx_interval_ms = DEFAULT_TX_INTERVAL_MS;


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

static int prepare_socket(void)
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