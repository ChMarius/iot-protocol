#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#define DEFAULT_TX_INTERVAL_MS 5000
#define MIN_TX_INTERVAL_MS     1000
#define MAX_TX_INTERVAL_MS     60000

void send_message(void);
void receive_ack(void);

#endif /* COMMUNICATION_H */