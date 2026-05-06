/**
 * Example Usage of IP Header Protocol
 * 
 * Grain Silo Sensor Data Example
 * Demonstrates flexible data types for temperature, humidity, and other sensors
 */

#include "ip_header.h"
#include <stdio.h>
#include <time.h>

/**
 * Example 1: Basic grain silo data with temperature and humidity
 */
void example_basic_grain_silo(void)
{
    printf("\n=== Example 1: Basic Grain Silo Data ===\n");

    ip_frame_t frame;
    char buffer[IP_HEADER_MAX_PAYLOAD];

    /* Initialize frame for device "silo-01" */
    if (ip_frame_init(&frame, "silo-01") != 0) {
        printf("ERROR: Failed to initialize frame\n");
        return;
    }

    /* Add temperature reading */
    if (ip_frame_add_variable(&frame, "temperature", IP_TYPE_NUMBER, "28.5",
                              SENSOR_TEMPERATURE) != 0) {
        printf("ERROR: Failed to add temperature\n");
        return;
    }
    ip_variable_set_unit(&frame, "C");
    ip_variable_set_timestamp(&frame, 1694567890000);

    /* Add humidity reading */
    if (ip_frame_add_variable(&frame, "humidity", IP_TYPE_NUMBER, "65.2",
                              SENSOR_HUMIDITY) != 0) {
        printf("ERROR: Failed to add humidity\n");
        return;
    }
    ip_variable_set_unit(&frame, "%");
    ip_variable_set_timestamp(&frame, 1694567890000);

    /* Add grain level */
    if (ip_frame_add_variable(&frame, "grain_level", IP_TYPE_NUMBER, "450",
                              SENSOR_LEVEL) != 0) {
        printf("ERROR: Failed to add grain level\n");
        return;
    }
    ip_variable_set_unit(&frame, "kg");

    /* Encode to text format (human-readable) */
    int encoded_len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), false);
    if (encoded_len > 0) {
        printf("Text Format: %s\n", buffer);
        printf("Encoded length: %d bytes\n\n", encoded_len);
    }

    printf("Variables in frame: %u\n", ip_frame_get_variable_count(&frame));
    for (uint16_t i = 0; i < ip_frame_get_variable_count(&frame); i++) {
        const ip_variable_t *var = ip_frame_get_variable(&frame, i);
        printf("  [%u] %s %s %s", i, var->name, ip_data_type_to_operator(var->type), var->value);
        if (var->has_unit)
            printf("#%s", var->unit);
        if (var->has_timestamp)
            printf("@%llu", (unsigned long long)var->timestamp);
        printf(" (sensor: %s)\n", ip_sensor_type_to_string(var->sensor_type));
    }
}

/**
 * Example 2: Binary encoding (compact format for bandwidth-limited networks)
 */
void example_binary_encoding(void)
{
    printf("\n=== Example 2: Binary Encoding ===\n");

    ip_frame_t frame;
    uint8_t binary_buffer[IP_HEADER_MAX_PAYLOAD];
    char text_buffer[256];

    ip_frame_init(&frame, "silo-02");

    /* Add temperature */
    ip_frame_add_variable(&frame, "temp", IP_TYPE_NUMBER, "32.1", SENSOR_TEMPERATURE);
    ip_variable_set_unit(&frame, "C");

    /* Add pressure */
    ip_frame_add_variable(&frame, "pressure", IP_TYPE_NUMBER, "1013.25", SENSOR_PRESSURE);
    ip_variable_set_unit(&frame, "hPa");

    /* Encode to binary */
    int binary_len = ip_frame_encode_binary(&frame, binary_buffer, sizeof(binary_buffer));
    if (binary_len > 0) {
        printf("Binary encoding length: %d bytes\n", binary_len);
        printf("Binary data (hex): ");
        for (int i = 0; i < binary_len; i++) {
            printf("%02x ", binary_buffer[i]);
        }
        printf("\n\n");

        /* Text encoding for comparison */
        int text_len = ip_frame_encode_text(&frame, text_buffer, sizeof(text_buffer), false);
        if (text_len > 0) {
            printf("Text encoding length: %d bytes\n", text_len);
            printf("Text format: %s\n", text_buffer);
            printf("Compression ratio: %.2f%%\n", (float)binary_len / text_len * 100);
        }
    }
}

/**
 * Example 3: Sequence counter for request/response correlation
 */
void example_sequence_counter(void)
{
    printf("\n=== Example 3: Sequence Counter ===\n");

    ip_frame_t frame;
    char buffer[IP_HEADER_MAX_PAYLOAD];

    ip_frame_init(&frame, "silo-03");

    /* Add sensor data */
    ip_frame_add_variable(&frame, "status", IP_TYPE_STRING, "running", SENSOR_STATUS);
    ip_frame_add_variable(&frame, "error_count", IP_TYPE_NUMBER, "0", SENSOR_COUNT);

    /* Set sequence number for tracking */
    ip_frame_set_sequence(&frame, 42);

    /* Encode with sequence */
    int encoded_len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), true);
    if (encoded_len > 0) {
        printf("With sequence counter:\n");
        printf("  %s\n\n", buffer);
    }

    /* Encode without sequence */
    encoded_len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), false);
    if (encoded_len > 0) {
        printf("Without sequence counter:\n");
        printf("  %s\n", buffer);
    }
}

/**
 * Example 4: Complex metadata and timestamps
 */
void example_advanced_metadata(void)
{
    printf("\n=== Example 4: Advanced Metadata ===\n");

    ip_frame_t frame;
    char buffer[IP_HEADER_MAX_PAYLOAD];

    ip_frame_init(&frame, "silo-04");

    /* Temperature with detailed metadata */
    ip_frame_add_variable(&frame, "temperature", IP_TYPE_NUMBER, "29.8", SENSOR_TEMPERATURE);
    ip_variable_set_unit(&frame, "C");
    ip_variable_set_timestamp(&frame, 1694567890123);
    ip_variable_set_metadata(&frame, "source=DHT22,quality=high,calibrated=true");

    /* Humidity with location */
    ip_frame_add_variable(&frame, "humidity", IP_TYPE_NUMBER, "72.5", SENSOR_HUMIDITY);
    ip_variable_set_unit(&frame, "%");
    ip_variable_set_metadata(&frame, "source=DHT22,quality=medium");

    int encoded_len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), false);
    if (encoded_len > 0) {
        printf("Frame with metadata:\n%s\n", buffer);
    }
}

/**
 * Example 5: Multiple sensor types for different use cases
 */
void example_multiple_sensors(void)
{
    printf("\n=== Example 5: Multiple Sensor Types ===\n");

    ip_frame_t frame;
    char buffer[IP_HEADER_MAX_PAYLOAD];

    ip_frame_init(&frame, "multi-sensor-01");

    /* Grain silo sensors */
    ip_frame_add_variable(&frame, "silo_temp", IP_TYPE_NUMBER, "25.3", SENSOR_TEMPERATURE);
    ip_variable_set_unit(&frame, "C");

    ip_frame_add_variable(&frame, "silo_humidity", IP_TYPE_NUMBER, "68", SENSOR_HUMIDITY);
    ip_variable_set_unit(&frame, "%");

    /* Environmental sensors */
    ip_frame_add_variable(&frame, "ambient_light", IP_TYPE_NUMBER, "450", SENSOR_LIGHT);
    ip_variable_set_unit(&frame, "lux");

    ip_frame_add_variable(&frame, "soil_moisture", IP_TYPE_NUMBER, "45.2", SENSOR_MOISTURE);
    ip_variable_set_unit(&frame, "%");

    /* Air quality */
    ip_frame_add_variable(&frame, "co2_level", IP_TYPE_NUMBER, "412", SENSOR_CO2);
    ip_variable_set_unit(&frame, "ppm");

    /* System status */
    ip_frame_add_variable(&frame, "device_status", IP_TYPE_STRING, "healthy", SENSOR_STATUS);
    ip_frame_add_variable(&frame, "battery", IP_TYPE_NUMBER, "87", SENSOR_COUNT);
    ip_variable_set_unit(&frame, "%");

    int encoded_len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), false);
    if (encoded_len > 0) {
        printf("Multi-sensor frame:\n%s\n\n", buffer);
        printf("Total variables: %u\n", ip_frame_get_variable_count(&frame));
    }
}

/**
 * Example 6: Binary round-trip (encode and decode)
 */
void example_roundtrip(void)
{
    printf("\n=== Example 6: Binary Round-Trip ===\n");

    ip_frame_t frame1, frame2;
    uint8_t binary_buffer[IP_HEADER_MAX_PAYLOAD];

    /* Create original frame */
    ip_frame_init(&frame1, "roundtrip-test");
    ip_frame_add_variable(&frame1, "value1", IP_TYPE_NUMBER, "42.5", SENSOR_TEMPERATURE);
    ip_variable_set_unit(&frame1, "C");
    ip_frame_add_variable(&frame1, "value2", IP_TYPE_STRING, "test_string", SENSOR_STATUS);
    ip_frame_set_sequence(&frame1, 99);

    /* Encode to binary */
    int binary_len = ip_frame_encode_binary(&frame1, binary_buffer, sizeof(binary_buffer));
    printf("Original frame encoded to %d bytes\n", binary_len);

    /* Decode back */
    if (ip_frame_decode_binary(binary_buffer, binary_len, &frame2) == 0) {
        printf("Successfully decoded frame\n");
        printf("  Device ID: %s\n", frame2.device_id);
        printf("  Variables: %u\n", ip_frame_get_variable_count(&frame2));
        printf("  Sequence: %u (has_seq: %s)\n", frame2.sequence_num,
               frame2.has_sequence ? "yes" : "no");

        for (uint16_t i = 0; i < ip_frame_get_variable_count(&frame2); i++) {
            const ip_variable_t *var = ip_frame_get_variable(&frame2, i);
            printf("    [%u] %s %s %s", i, var->name, 
                   ip_data_type_to_operator(var->type), var->value);
            if (var->has_unit)
                printf("#%s", var->unit);
            printf("\n");
        }
    } else {
        printf("ERROR: Failed to decode binary frame\n");
    }
}

int main(void)
{
    printf("IP Header Protocol - Usage Examples\n");
    printf("===================================\n");

    example_basic_grain_silo();
    example_binary_encoding();
    example_sequence_counter();
    example_advanced_metadata();
    example_multiple_sensors();
    example_roundtrip();

    printf("\n=== Examples Complete ===\n");

    return 0;
}
