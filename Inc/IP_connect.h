#ifndef IP_CONNECT_H
#define IP_CONNECT_H

#define WIFI_SSID    "UK-18"
#define WIFI_PSK     "z6r467Mg"

#define SERVER_IP    ""
#define SERVER_PORT  5000

#define ROTATION_INTERVAL 5 /* rotate ports every n sends*/

int wifi_connect(void);
void init_server_addr(void);
int prepare_socket(void);

#endif /* IP_CONNECT_H */