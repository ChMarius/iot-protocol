/**
 * Integration Example: IP Header Protocol with Your WiFi Setup
 * 
 * This shows how to integrate the IP Header Protocol into your existing
 * Zephyr + WiFi + UDP socket code for the RPi Pico W
 */

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include "ip_header.h"
#include <stdio.h>
#include <time.h>

/* ==================== Data Collection Simulation ==================== */

/**
 * Simulate reading from grain silo sensors
 * In real implementation, these would read from actual ADC/I2C sensors
 */

float read_grain_temperature(void)
{
    /* TODO: Replace with actual sensor read
     * Example: Read from DHT22 via I2C/GPIO
     * return dht22_read_temperature();
     */
    return 28.5f;  // Placeholder: 28.5°C
}

float read_grain_humidity(void)
{
    /* TODO: Replace with actual sensor read */
    return 65.2f;  // Placeholder: 65.2% RH
}

float read_grain_level(void)
{
    /* TODO: Replace with actual sensor read
     * Could be weight sensor, ultrasonic level sensor, etc.
     */
    return 850.0f;  // Placeholder: 850 kg
}

int read_silo_status(void)
{
    /* TODO: Check system health, motor status, etc. */
    return 0;  // 0 = ok, 1 = warning, 2 = error
}

const char *get_status_string(int status_code)
{
    switch (status_code) {
        case 0: return "operational";
        case 1: return "warning";
        case 2: return "error";
        default: return "unknown";
    }
}

/* ==================== IP Header Frame Building ==================== */

/**
 * Build a complete grain silo data frame
 * 
 * This function demonstrates how to:
 * 1. Initialize a frame
 * 2. Add multiple sensor variables
 * 3. Set units and metadata
 * 4. Set sequence counter
 */
int build_grain_silo_frame(ip_frame_t *frame, const char *device_id, uint32_t sequence)
{
    char metadata_buf[100];
    char status_buf[32];

    /* Initialize frame with device identifier */
    if (ip_frame_init(frame, device_id) != 0) {
        printk("ERROR: Failed to initialize frame\n");
        return -1;
    }

    /* === Primary Measurements: Grain Temperature === */
    float grain_temp = read_grain_temperature();
    char temp_str[20];
    snprintf(temp_str, sizeof(temp_str), "%.1f", grain_temp);

    if (ip_frame_add_variable(frame, "grain_temperature", 
                              IP_TYPE_NUMBER, temp_str, 
                              SENSOR_TEMPERATURE) != 0) {
        printk("ERROR: Failed to add temperature\n");
        return -1;
    }

    /* Set unit and metadata */
    ip_variable_set_unit(frame, "C");
    snprintf(metadata_buf, sizeof(metadata_buf), 
             "sensor=DHT22,location=top,calibrated=true");
    ip_variable_set_metadata(frame, metadata_buf);

    /* === Primary Measurements: Grain Humidity === */
    float grain_humidity = read_grain_humidity();
    char humidity_str[20];
    snprintf(humidity_str, sizeof(humidity_str), "%.1f", grain_humidity);

    if (ip_frame_add_variable(frame, "grain_humidity", 
                              IP_TYPE_NUMBER, humidity_str, 
                              SENSOR_HUMIDITY) != 0) {
        printk("ERROR: Failed to add humidity\n");
        return -1;
    }

    ip_variable_set_unit(frame, "%");
    snprintf(metadata_buf, sizeof(metadata_buf), 
             "sensor=DHT22,location=middle");
    ip_variable_set_metadata(frame, metadata_buf);

    /* === Inventory: Grain Level === */
    float grain_level = read_grain_level();
    char level_str[20];
    snprintf(level_str, sizeof(level_str), "%.0f", grain_level);

    if (ip_frame_add_variable(frame, "grain_level", 
                              IP_TYPE_NUMBER, level_str, 
                              SENSOR_LEVEL) != 0) {
        printk("ERROR: Failed to add grain level\n");
        return -1;
    }

    ip_variable_set_unit(frame, "kg");
    ip_variable_set_metadata(frame, "sensor=load_cell");

    /* === System Status === */
    int status = read_silo_status();
    if (ip_frame_add_variable(frame, "silo_status", 
                              IP_TYPE_STRING, get_status_string(status), 
                              SENSOR_STATUS) != 0) {
        printk("ERROR: Failed to add status\n");
        return -1;
    }

    /* === Optional: Additional Sensor Data === */
    /* Uncomment to add more sensors */

    // /* Barometric pressure (indicates weather/altitude) */
    // char pressure_str[20];
    // snprintf(pressure_str, sizeof(pressure_str), "1013.25");
    // ip_frame_add_variable(frame, "pressure", IP_TYPE_NUMBER, pressure_str, SENSOR_PRESSURE);
    // ip_variable_set_unit(frame, "hPa");

    // /* Battery level */
    // char battery_str[20];
    // snprintf(battery_str, sizeof(battery_str), "92");
    // ip_frame_add_variable(frame, "battery", IP_TYPE_NUMBER, battery_str, SENSOR_COUNT);
    // ip_variable_set_unit(frame, "%");

    /* Set sequence counter for request correlation */
    ip_frame_set_sequence(frame, sequence);

    return 0;
}

/* ==================== Encoding Functions ==================== */

/**
 * Encode frame to text format (human-readable)
 * Use for debugging or when bandwidth is less critical
 */
int encode_to_text(ip_frame_t *frame, char *buffer, size_t buffer_size)
{
    return ip_frame_encode_text(frame, buffer, buffer_size, true);
}

/**
 * Encode frame to binary format (compact)
 * Use for bandwidth-constrained networks
 */
int encode_to_binary(ip_frame_t *frame, uint8_t *buffer, size_t buffer_size)
{
    return ip_frame_encode_binary(frame, buffer, buffer_size);
}

/* ==================== Network Transmission ==================== */

/**
 * Send IP Header frame over UDP to server
 * Integrated with your existing WiFi socket code
 * 
 * @param sock UDP socket file descriptor
 * @param server Server address structure
 * @param use_binary If true, use binary encoding; else use text
 * @return Bytes sent on success, -1 on error
 */
int send_ip_header_frame(int sock, struct sockaddr_in *server, 
                         const char *device_id, uint32_t sequence,
                         bool use_binary)
{
    ip_frame_t frame;
    int encoded_len = 0;

    /* Build frame with sensor data */
    if (build_grain_silo_frame(&frame, device_id, sequence) != 0) {
        printk("ERROR: Failed to build frame\n");
        return -1;
    }

    printk("Frame built with %u variables\n", ip_frame_get_variable_count(&frame));

    if (use_binary) {
        /* === Binary Encoding (Compact) === */
        uint8_t binary_buffer[512];
        
        encoded_len = encode_to_binary(&frame, binary_buffer, sizeof(binary_buffer));
        if (encoded_len <= 0) {
            printk("ERROR: Binary encoding failed\n");
            return -1;
        }

        printk("Sending binary frame (%d bytes)\n", encoded_len);

        /* Send binary data */
        int sent = sendto(sock, binary_buffer, encoded_len, 0,
                         (struct sockaddr *)server, sizeof(*server));
        
        if (sent < 0) {
            printk("ERROR: sendto failed with code %d\n", sent);
            return -1;
        }

        return sent;

    } else {
        /* === Text Encoding (Human-Readable) === */
        char text_buffer[512];
        
        encoded_len = encode_to_text(&frame, text_buffer, sizeof(text_buffer));
        if (encoded_len <= 0) {
            printk("ERROR: Text encoding failed\n");
            return -1;
        }

        printk("Sending text frame (%d bytes)\n", encoded_len);
        printk("Payload: %s\n", text_buffer);

        /* Send text data */
        int sent = sendto(sock, text_buffer, encoded_len, 0,
                         (struct sockaddr *)server, sizeof(*server));
        
        if (sent < 0) {
            printk("ERROR: sendto failed with code %d\n", sent);
            return -1;
        }

        return sent;
    }
}

/* ==================== Integration with Your Main Thread ==================== */

/**
 * Replace your existing send_data() function with this enhanced version
 * This integrates IP Header Protocol with your WiFi setup
 */
void send_grain_silo_data(int sock, struct sockaddr_in *server)
{
    static uint32_t send_count = 0;

    /* Choose encoding based on your needs:
     * - Binary for bandwidth-limited networks (LoRa, cellular)
     * - Text for debugging or WiFi with good bandwidth
     */
    bool use_binary = false;  // Set to true for maximum compression

    printk("\n--- Sending Grain Silo Data (Message #%u) ---\n", send_count);

    int bytes_sent = send_ip_header_frame(sock, server, 
                                          "silo-warehouse-01",  // Device ID
                                          send_count++,          // Sequence
                                          use_binary);           // Encoding

    if (bytes_sent > 0) {
        printk("SUCCESS: Sent %d bytes\n", bytes_sent);
    } else {
        printk("FAILED: Could not send data\n");
    }
}

/* ==================== Periodic Sensor Collection Thread ==================== */

/**
 * Sensor collection thread
 * Reads sensors and sends data at regular intervals
 * 
 * Integration point in your main.c:
 * 
 * In main():
 *     k_thread_create(&sensor_thread_id, sensor_thread_stack,
 *                     K_THREAD_STACK_SIZEOF(sensor_thread_stack),
 *                     sensor_collection_thread,
 *                     (int)(uintptr_t)sock, (int)(uintptr_t)server, NULL,
 *                     2, K_PRIO_COOP_USER, K_FOREVER);
 *     k_thread_start(&sensor_thread_id);
 */

#define SENSOR_READ_INTERVAL_MS 60000  /* Read sensors every 60 seconds */

K_THREAD_STACK_DEFINE(sensor_thread_stack, 2048);
struct k_thread sensor_thread_id;

void sensor_collection_thread(void *arg1, void *arg2, void *arg3)
{
    int sock = (int)(uintptr_t)arg1;
    struct sockaddr_in *server = (struct sockaddr_in *)arg2;

    printk("Sensor collection thread started\n");

    while (1) {
        /* Wait for interval */
        k_msleep(SENSOR_READ_INTERVAL_MS);

        /* Check WiFi is connected (from your existing code) */
        if (!wifi_connected) {
            printk("WiFi not connected, skipping send\n");
            continue;
        }

        /* Send sensor data */
        send_grain_silo_data(sock, server);
    }
}

/* ==================== Integration Checklist ==================== */

/*
 * To integrate IP Header Protocol into your project:
 * 
 * 1. FILE SETUP
 *    [ ] Copy ip_header.h to src/
 *    [ ] Copy ip_header.c to src/
 *    [ ] Update CMakeLists.txt:
 *        target_sources(app PRIVATE src/ip_header.c)
 * 
 * 2. HEADER INCLUDES
 *    [ ] Add to main.c:
 *        #include "ip_header.h"
 * 
 * 3. SENSOR CALIBRATION
 *    [ ] Replace read_grain_temperature() with actual sensor code
 *    [ ] Replace read_grain_humidity() with actual sensor code
 *    [ ] Replace read_grain_level() with actual sensor code
 *    [ ] Replace read_silo_status() with actual system checks
 * 
 * 4. THREAD CREATION
 *    [ ] Create sensor collection thread in main()
 *    [ ] Pass socket and server address to thread
 * 
 * 5. MAIN LOOP
 *    [ ] Optionally call send_grain_silo_data() on demand
 *    [ ] Or let sensor thread handle periodic sends
 * 
 * 6. TESTING
 *    [ ] Start your server to receive data
 *    [ ] Monitor WiFi connection
 *    [ ] Verify sensor data is being sent
 *    [ ] Check receive format (text/binary)
 * 
 * 7. OPTIMIZATION
 *    [ ] Toggle use_binary flag for compression testing
 *    [ ] Monitor power consumption
 *    [ ] Adjust SENSOR_READ_INTERVAL_MS as needed
 * 
 * 8. DEBUGGING
 *    [ ] Decode received binary frames to verify integrity
 *    [ ] Use text format initially for easy debugging
 *    [ ] Implement receive ACK handler for flow control
 */

/* ==================== Alternative: Batch Sending ==================== */

/**
 * Optional: Collect multiple readings and send as batch
 * Useful for reducing WiFi power consumption
 */
void send_batch_readings(int sock, struct sockaddr_in *server, int batch_count)
{
    ip_frame_t frame;
    uint8_t buffer[1024];
    static uint32_t seq = 0;

    ip_frame_init(&frame, "silo-warehouse-batch");

    /* Collect multiple readings */
    for (int i = 0; i < batch_count; i++) {
        char value[20];
        
        /* Each reading becomes a separate variable */
        snprintf(value, sizeof(value), "%.1f", read_grain_temperature());
        ip_frame_add_variable(&frame, "temp_reading", IP_TYPE_NUMBER, 
                              value, SENSOR_TEMPERATURE);
        ip_variable_set_unit(&frame, "C");
        
        k_msleep(1000);  /* Delay between readings */
    }

    ip_frame_set_sequence(&frame, seq++);

    /* Send as single batch */
    int len = ip_frame_encode_binary(&frame, buffer, sizeof(buffer));
    if (len > 0) {
        sendto(sock, buffer, len, 0, (struct sockaddr *)server, sizeof(*server));
    }
}

/* ==================== Example: Usage in Your main.c ==================== */

/*
 * Add this to your existing main() function:
 * 
 * void main(void)
 * {
 *     // ... existing WiFi setup code ...
 * 
 *     // Create and bind socket
 *     sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
 *     // ... configure socket ...
 * 
 *     // Start sensor collection thread
 *     k_thread_create(&sensor_thread_id, sensor_thread_stack,
 *                     K_THREAD_STACK_SIZEOF(sensor_thread_stack),
 *                     sensor_collection_thread,
 *                     (void *)(uintptr_t)sock, (void *)&server, NULL,
 *                     2, K_PRIO_COOP_USER, K_FOREVER);
 *     k_thread_start(&sensor_thread_id);
 * 
 *     // Main thread can do other work here
 * }
 */
