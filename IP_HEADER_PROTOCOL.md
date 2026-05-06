# IP Header Protocol Documentation

## Overview

The **IP Header Protocol** is a flexible, lightweight IoT messaging format inspired by TagoTiP (Transport IoT Protocol). It's designed specifically for resource-constrained embedded devices, with a primary use case for grain silo monitoring but extensible to any IoT application.

### Key Features

- **Flexible Data Types**: Support for numbers, strings, booleans, and locations
- **Multiple Sensor Types**: Pre-defined sensor classifications (temperature, humidity, pressure, etc.)
- **Dual Encoding**: Human-readable text format AND compact binary format
- **Minimal Overhead**: ~4-5x smaller than HTTP/JSON when using binary encoding
- **Extensible**: Add custom sensor types as needed
- **Sequence Support**: Optional request/response correlation tracking
- **Metadata Support**: Attach rich metadata to measurements (source, quality, calibration info)
- **Timestamps**: Include precise timing information with each measurement

---

## Data Model

### Frame Structure

```
PUSH|[!sequence]|device_id|[variable1;variable2;...]
```

Example:
```
PUSH|silo-01|[temperature:=28.5#C@1694567890000;humidity:=65.2#%]
PUSH|!42|silo-02|[temp:=32.1#C{source=DHT22,quality=high}]
```

### Variable Syntax

Each variable follows the pattern:
```
name operator value [#unit] [@timestamp] [^group] [{metadata}]
```

**Operators:**
- `:=` → Number (e.g., `temperature:=28.5`)
- `=` → String (e.g., `status=running`)
- `?=` → Boolean (e.g., `active?=true`)
- `@=` → Location (e.g., `position@=39.74,-104.99,305`)

**Suffixes (all optional):**
- `#unit` → Unit of measurement (e.g., `#C`, `#%`, `#ppm`)
- `@timestamp` → Timestamp in milliseconds
- `{key=value,...}` → Metadata pairs

---

## Sensor Types

Pre-defined sensor classifications for common IoT scenarios:

| Type | Usage | Example |
|------|-------|---------|
| `SENSOR_TEMPERATURE` | Temperature readings | Grain silo, ambient |
| `SENSOR_HUMIDITY` | Moisture/humidity levels | Grain silo, air |
| `SENSOR_PRESSURE` | Barometric pressure | Weather, altitude |
| `SENSOR_MOISTURE` | Soil/material moisture | Agriculture |
| `SENSOR_LIGHT` | Light intensity | Environment monitoring |
| `SENSOR_CO2` | CO2 concentration | Air quality |
| `SENSOR_GAS` | Generic gas sensor | Air quality, safety |
| `SENSOR_LEVEL` | Material/liquid level | Grain silos, tanks |
| `SENSOR_WEIGHT` | Weight/mass measurement | Scale, inventory |
| `SENSOR_COUNT` | Generic counter | Errors, events |
| `SENSOR_STATUS` | System status | Device health |
| `SENSOR_CUSTOM` | User-defined | Application-specific |

---

## Grain Silo Use Case

### Typical Measurements

```c
/* Initialize frame for silo device */
ip_frame_t frame;
ip_frame_init(&frame, "silo-warehouse-01");

/* Temperature (critical for grain preservation) */
ip_frame_add_variable(&frame, "grain_temp", IP_TYPE_NUMBER, "24.5", SENSOR_TEMPERATURE);
ip_variable_set_unit(&frame, "C");
ip_variable_set_timestamp(&frame, time_ms);
ip_variable_set_metadata(&frame, "sensor=DHT22,location=top");

/* Humidity (prevents mold and degradation) */
ip_frame_add_variable(&frame, "grain_humidity", IP_TYPE_NUMBER, "68.2", SENSOR_HUMIDITY);
ip_variable_set_unit(&frame, "%");
ip_variable_set_metadata(&frame, "sensor=DHT22,location=middle");

/* Grain Level (inventory tracking) */
ip_frame_add_variable(&frame, "grain_level", IP_TYPE_NUMBER, "850", SENSOR_LEVEL);
ip_variable_set_unit(&frame, "kg");

/* System Status */
ip_frame_add_variable(&frame, "silo_status", IP_TYPE_STRING, "operational", SENSOR_STATUS);
ip_frame_add_variable(&frame, "last_maintenance", IP_TYPE_NUMBER, "5", SENSOR_COUNT);
ip_variable_set_unit(&frame, "days");
```

**Text output:**
```
PUSH|silo-warehouse-01|[grain_temp:=24.5#C@1694567890000{sensor=DHT22,location=top};grain_humidity:=68.2#%{sensor=DHT22,location=middle};grain_level:=850#kg;silo_status=operational;last_maintenance:=5#days]
```

---

## API Reference

### Frame Initialization

```c
int ip_frame_init(ip_frame_t *frame, const char *device_id);
```

Initialize a new frame with device identifier.

**Parameters:**
- `frame`: Pointer to frame structure
- `device_id`: Unique device identifier (max 50 chars)

**Returns:** 0 on success, -1 on error

---

### Adding Variables

```c
int ip_frame_add_variable(ip_frame_t *frame, const char *name,
                          ip_data_type_t type, const char *value,
                          sensor_type_t sensor_type);
```

Add a measurement to the frame.

**Parameters:**
- `frame`: Pointer to frame structure
- `name`: Variable name (alphanumeric + underscore, max 50 chars)
- `type`: Data type (IP_TYPE_NUMBER, IP_TYPE_STRING, etc.)
- `value`: Variable value as string
- `sensor_type`: Sensor classification

**Returns:** 0 on success, -1 if frame full or invalid input

**Example:**
```c
ip_frame_add_variable(&frame, "temperature", IP_TYPE_NUMBER, "28.5", SENSOR_TEMPERATURE);
```

---

### Variable Suffixes

```c
int ip_variable_set_unit(ip_frame_t *frame, const char *unit);
int ip_variable_set_timestamp(ip_frame_t *frame, uint64_t timestamp_ms);
int ip_variable_set_metadata(ip_frame_t *frame, const char *metadata);
```

Add optional suffixes to the last added variable.

**Example:**
```c
ip_frame_add_variable(&frame, "temp", IP_TYPE_NUMBER, "32.5", SENSOR_TEMPERATURE);
ip_variable_set_unit(&frame, "C");
ip_variable_set_timestamp(&frame, 1694567890000);
ip_variable_set_metadata(&frame, "source=DHT22,quality=high");
```

---

### Frame Encoding

#### Text Format (Human-Readable)

```c
int ip_frame_encode_text(const ip_frame_t *frame, char *buffer,
                         size_t buffer_size, bool add_sequence);
```

Encode frame to readable text format (similar to TagoTiP).

**Parameters:**
- `frame`: Pointer to frame structure
- `buffer`: Output buffer
- `buffer_size`: Size of output buffer
- `add_sequence`: Include sequence counter in output

**Returns:** Length of encoded data on success, -1 on error

**Example:**
```c
char text_buffer[512];
int len = ip_frame_encode_text(&frame, text_buffer, sizeof(text_buffer), false);
if (len > 0) {
    printf("Encoded: %s\n", text_buffer);
}
```

#### Binary Format (Compact)

```c
int ip_frame_encode_binary(const ip_frame_t *frame, uint8_t *buffer,
                           size_t buffer_size);
```

Encode frame to compact binary format for bandwidth-limited networks.

**Binary Layout:**
```
[Version:1B][DeviceIDLen:1B][DeviceID:NB][HasSeq:1B][Seq:4B(opt)]
[VarCount:1B]
[For each variable:
  [Type:1B][SensorType:1B][NameLen:1B][Name:NB]
  [ValueLen:2B][Value:NB]
  [UnitLen:1B][Unit:NB(opt)]
  [HasTimestamp:1B][Timestamp:8B(opt)]
  [MetaLen:2B][Metadata:NB(opt)]
]
```

**Returns:** Length of encoded data on success, -1 on error

---

### Frame Decoding

```c
int ip_frame_decode_text(const char *buffer, size_t buffer_size,
                         ip_frame_t *frame);
int ip_frame_decode_binary(const uint8_t *buffer, size_t buffer_size,
                           ip_frame_t *frame);
```

Parse encoded frame data back into structure.

**Returns:** 0 on success, -1 on parse error

---

### Sequence Counter

```c
void ip_frame_set_sequence(ip_frame_t *frame, uint32_t seq_num);
```

Add sequence counter for request/response correlation.

**Example:**
```c
ip_frame_set_sequence(&frame, 42);
// Output: PUSH|!42|device-01|[temp:=32]
```

---

### Utility Functions

```c
void ip_frame_clear_variables(ip_frame_t *frame);
uint16_t ip_frame_get_variable_count(const ip_frame_t *frame);
const ip_variable_t *ip_frame_get_variable(const ip_frame_t *frame, uint16_t index);
const char *ip_sensor_type_to_string(sensor_type_t sensor_type);
const char *ip_data_type_to_operator(ip_data_type_t type);
```

---

## Constants

```c
#define IP_HEADER_MAX_VARIABLES   32    /* Max variables per message */
#define IP_HEADER_MAX_VAR_NAME    50    /* Max variable name length */
#define IP_HEADER_MAX_VAR_VALUE   256   /* Max variable value length */
#define IP_HEADER_MAX_UNIT        25    /* Max unit string length */
#define IP_HEADER_MAX_PAYLOAD     512   /* Max total payload size */
```

---

## Encoding Comparison

### Example: Grain Silo Data

**Original (HTTP/JSON):**
```json
{
  "device_id": "silo-01",
  "timestamp": 1694567890000,
  "data": [
    {
      "name": "temperature",
      "value": 28.5,
      "unit": "C",
      "sensor_type": "DHT22"
    },
    {
      "name": "humidity",
      "value": 65.2,
      "unit": "%",
      "sensor_type": "DHT22"
    }
  ]
}
```
**Size: ~220 bytes**

**IP Header (Text):**
```
PUSH|silo-01|[temperature:=28.5#C{sensor=DHT22};humidity:=65.2#%{sensor=DHT22}]
```
**Size: ~95 bytes** (43% of JSON)

**IP Header (Binary):**
```
01 07 73 69 6c 6f 2d 30 31 00 02 3a 00 0b 74 65 6d 70 65 72 61 74 75 72 65 00 04 32 38 2e 35 ...
```
**Size: ~65 bytes** (30% of JSON)

**Compression:** 4.7x smaller than HTTP/JSON with binary encoding

---

## Integration Example: WiFi Sensor Upload

```c
#include "ip_header.h"
#include <zephyr/net/socket.h>

void send_grain_silo_data(int sock, struct sockaddr_in *server)
{
    ip_frame_t frame;
    char text_buffer[512];
    uint8_t binary_buffer[512];

    /* Create frame */
    ip_frame_init(&frame, "silo-warehouse-01");

    /* Add measurements */
    ip_frame_add_variable(&frame, "temperature", IP_TYPE_NUMBER, "28.5", SENSOR_TEMPERATURE);
    ip_variable_set_unit(&frame, "C");

    ip_frame_add_variable(&frame, "humidity", IP_TYPE_NUMBER, "65.2", SENSOR_HUMIDITY);
    ip_variable_set_unit(&frame, "%");

    ip_frame_add_variable(&frame, "grain_level", IP_TYPE_NUMBER, "850", SENSOR_LEVEL);
    ip_variable_set_unit(&frame, "kg");

    /* Set sequence for correlation */
    ip_frame_set_sequence(&frame, send_count++);

    /* Choose encoding based on bandwidth */
    #ifdef USE_BINARY_ENCODING
    int encoded_len = ip_frame_encode_binary(&frame, binary_buffer, sizeof(binary_buffer));
    if (encoded_len > 0) {
        sendto(sock, binary_buffer, encoded_len, 0,
               (struct sockaddr *)server, sizeof(*server));
    }
    #else
    int encoded_len = ip_frame_encode_text(&frame, text_buffer, sizeof(text_buffer), true);
    if (encoded_len > 0) {
        sendto(sock, text_buffer, encoded_len, 0,
               (struct sockaddr *)server, sizeof(*server));
    }
    #endif
}
```

---

## Design Principles (from TagoTiP)

1. **Type-Safe**: Explicit operators for different data types
2. **Compact**: Minimal overhead for bandwidth-limited networks
3. **Transport-Agnostic**: Works over UDP, TCP, HTTP, MQTT, etc.
4. **C-Friendly**: Linear parsing, predictable buffer sizes
5. **Extensible**: Support for custom sensor types and metadata
6. **Human-Readable**: Text format can be read directly
7. **Flexible**: Optional suffixes for rich metadata without bloat

---

## Future Enhancements

- [ ] Encryption support (TagoTiP/S-like envelope)
- [ ] Compression for very large payloads
- [ ] Multi-device aggregation
- [ ] Standardized error codes and status messages
- [ ] Time-series batch uploads
- [ ] Geographic location data support

---

## References

- **TagoTiP**: https://github.com/tago-io/tagotip
- **RFC 5234**: ABNF (Augmented Backus-Naur Form)
- **IoT Best Practices**: Lightweight protocols for resource-constrained devices

---

## License

IP Header Protocol is provided as-is for your thesis project.
Adapt and extend as needed for your specific requirements.
