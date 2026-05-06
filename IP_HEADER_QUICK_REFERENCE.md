# IP Header Protocol - Quick Reference

## Frame Format

```
PUSH | [!sequence] | device_id | [variable1; variable2; ...]
```

## Data Type Operators

| Operator | Type | Example |
|----------|------|---------|
| `:=` | Number | `temperature:=28.5` |
| `=` | String | `status=running` |
| `?=` | Boolean | `active?=true` |
| `@=` | Location | `position@=39.74,-104.99,305` |

## Optional Suffixes

```
name operator value [#unit] [@timestamp] [^group] [{metadata}]
```

| Suffix | Purpose | Example |
|--------|---------|---------|
| `#` | Unit | `#C`, `#%`, `#kg` |
| `@` | Timestamp (ms) | `@1694567890000` |
| `^` | Group | `^reading_001` |
| `{}` | Metadata (key=value pairs) | `{source=DHT22,quality=high}` |

## Common Sensor Types

```c
SENSOR_TEMPERATURE      // °C, °F, K
SENSOR_HUMIDITY         // %RH
SENSOR_PRESSURE         // hPa, Pa, mmHg
SENSOR_MOISTURE         // %
SENSOR_LIGHT            // lux, cd/m²
SENSOR_CO2              // ppm
SENSOR_GAS              // ppm, ppb
SENSOR_LEVEL            // kg, L, m
SENSOR_WEIGHT           // kg, g, lb
SENSOR_COUNT            // generic counter
SENSOR_STATUS           // operational, error, etc
SENSOR_CUSTOM           // user-defined
```

## Code Examples

### Basic Setup

```c
#include "ip_header.h"

ip_frame_t frame;
ip_frame_init(&frame, "device-01");
```

### Add Measurements

```c
// Temperature with unit
ip_frame_add_variable(&frame, "temp", IP_TYPE_NUMBER, "28.5", SENSOR_TEMPERATURE);
ip_variable_set_unit(&frame, "C");

// Humidity
ip_frame_add_variable(&frame, "humidity", IP_TYPE_NUMBER, "65", SENSOR_HUMIDITY);
ip_variable_set_unit(&frame, "%");

// Status
ip_frame_add_variable(&frame, "status", IP_TYPE_STRING, "ok", SENSOR_STATUS);
```

### Add Timestamps & Metadata

```c
// Timestamp (milliseconds since epoch)
ip_variable_set_timestamp(&frame, 1694567890000);

// Metadata
ip_variable_set_metadata(&frame, "source=DHT22,quality=high");
```

### Encoding Options

**Text Format (Human-Readable):**
```c
char buffer[512];
int len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), false);
// Output: PUSH|device-01|[temp:=28.5#C;humidity:=65#%]
```

**Binary Format (Compact):**
```c
uint8_t buffer[512];
int len = ip_frame_encode_binary(&frame, buffer, sizeof(buffer));
// 60-80 bytes vs 100+ bytes for text
```

**With Sequence Counter:**
```c
ip_frame_set_sequence(&frame, 42);
int len = ip_frame_encode_text(&frame, buffer, sizeof(buffer), true);
// Output: PUSH|!42|device-01|[temp:=28.5#C]
```

## Grain Silo Template

```c
void collect_grain_silo_data(ip_frame_t *frame, const char *silo_id)
{
    // Initialize
    ip_frame_init(frame, silo_id);

    // Temperature monitoring
    ip_frame_add_variable(frame, "grain_temperature", 
                         IP_TYPE_NUMBER, read_temperature(), 
                         SENSOR_TEMPERATURE);
    ip_variable_set_unit(frame, "C");

    // Humidity monitoring
    ip_frame_add_variable(frame, "grain_humidity", 
                         IP_TYPE_NUMBER, read_humidity(), 
                         SENSOR_HUMIDITY);
    ip_variable_set_unit(frame, "%");

    // Level monitoring
    ip_frame_add_variable(frame, "grain_level", 
                         IP_TYPE_NUMBER, read_level(), 
                         SENSOR_LEVEL);
    ip_variable_set_unit(frame, "kg");

    // System status
    ip_frame_add_variable(frame, "silo_status", 
                         IP_TYPE_STRING, get_status(), 
                         SENSOR_STATUS);

    // Optional: Add metadata
    char metadata[100];
    snprintf(metadata, sizeof(metadata), "hwver=%s,fwver=%s", 
             get_hardware_version(), get_firmware_version());
    ip_variable_set_metadata(frame, metadata);
}
```

## Size Limits

```c
#define IP_HEADER_MAX_VARIABLES   32      // Max 32 variables per message
#define IP_HEADER_MAX_VAR_NAME    50      // Max 50 chars for variable name
#define IP_HEADER_MAX_VAR_VALUE   256     // Max 256 chars for value
#define IP_HEADER_MAX_UNIT        25      // Max 25 chars for unit
#define IP_HEADER_MAX_PAYLOAD     512     // Max 512 bytes total
```

## Typical Bandwidth Savings

| Format | Size | Relative |
|--------|------|----------|
| HTTP/JSON | 250+ bytes | 100% |
| IP Text | 95 bytes | 38% |
| IP Binary | 60 bytes | 24% |

**Compression Ratio: 4-5x smaller than HTTP/JSON**

## Integration Checklist

- [ ] Include `ip_header.h` in project
- [ ] Compile `ip_header.c` with project
- [ ] Initialize frame with device ID
- [ ] Add sensor variables with appropriate types
- [ ] Set units for measurements
- [ ] Add timestamps if time sync available
- [ ] Choose text or binary encoding
- [ ] Send over UDP/TCP/MQTT
- [ ] Implement error handling for failed frames

## Common Patterns

### Periodic Sensor Reading

```c
void sensor_thread(void)
{
    ip_frame_t frame;
    static uint32_t seq = 0;

    while (1) {
        k_sleep(K_SECONDS(60)); // Read every minute

        ip_frame_init(&frame, "silo-01");
        
        // Collect sensor data
        read_and_add_sensors(&frame);
        
        // Add sequence
        ip_frame_set_sequence(&frame, seq++);
        
        // Send
        char buffer[512];
        if (ip_frame_encode_text(&frame, buffer, sizeof(buffer), true) > 0) {
            send_to_server(buffer);
        }
    }
}
```

### Batch Upload

```c
ip_frame_t frame;
ip_frame_init(&frame, "silo-01");

// Add multiple readings
for (int i = 0; i < 10; i++) {
    read_sensor_into_frame(&frame, i);
}

// Send all at once
char buffer[2048];
if (ip_frame_encode_binary(&frame, (uint8_t*)buffer, sizeof(buffer)) > 0) {
    send_batch_to_server(buffer);
}
```

### Response Correlation

```c
// Send request with sequence
ip_frame_set_sequence(&frame, 123);
send_frame(&frame);

// Receive response with matching sequence
ip_frame_t response;
if (response.sequence_num == 123 && response.has_sequence) {
    // Match found - this is our response
    process_response(&response);
}
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Buffer overflow | Increase `IP_HEADER_MAX_PAYLOAD` or reduce variables/values |
| Name too long | Keep variable names under 50 characters |
| Frame not encoding | Check `variable_count` is > 0, all names valid |
| Binary decode fails | Verify binary format not corrupted in transmission |
| Device not receiving | Check device_id matches receiver expectations |

## ASCII Character Reference

```c
IP_HEADER_FRAME_DELIM   '|'   // Field separator
IP_HEADER_VAR_DELIM     ';'   // Variable separator
IP_HEADER_BLOCK_OPEN    '['   // Variable block start
IP_HEADER_BLOCK_CLOSE   ']'   // Variable block end
```

## Related Standards

- **TagoTiP** (tago-io/tagotip): Reference implementation
- **RFC 5234**: ABNF grammar
- **ISO 8601**: Timestamp format
- **SI Units**: Standard measurement units
