#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>

#define WIFI_SSID    "UK-18"
#define WIFI_PSK     "z6r467Mg"

#define SERVER_IP    ""
#define SERVER_PORT  5000

static int sock;
static struct sockaddr_in server;

/* WiFi state tracking */
static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static bool wifi_connected = false;

/* WiFi event callback */
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback dhcp_cb;

/* Add this semaphore alongside your existing wifi one */
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
        .mfp         = WIFI_MFP_OPTIONAL,
    };

    printk("wifi: connecting to %s...\n", WIFI_SSID);

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
                       &params, sizeof(params));
    if (ret != 0) {
        printk("wifi: connect request failed (%d)\n", ret);
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

void open_socket(void)
{
    /* Small delay to let the network stack fully settle after DHCP */
    k_msleep(500);

    sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    /* In Zephyr, valid fds start at 0 but -1 or negative means error */
    /* Check errno to confirm */
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
        return;
    }

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

void close_socket(void)
{
    zsock_close(sock);
    printk("Socket closed (fd=%d released)\n", sock);
    sock = -1;
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
    
    init_server_addr();
    while(1)
    {
       open_socket();
       send_message();
       receive_ack();
       close_socket();
       k_msleep(5000);
    }
    
    return 0;
}