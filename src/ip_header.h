#ifndef IP_HEADER_H
#define IP_HEADER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/**
 * IP Header Protocol - Flexible IoT Payload Format
 * Inspired by TagoTiP (Transport IoT Protocol)
 * 
 * Designed for grain silo sensors (temperature, humidity, etc.)
 * while remaining flexible for multiple sensor types and data formats.
 */

/* ==================== Constants ==================== */

#define IP_HEADER_VERSION       1
#define IP_HEADER_MAX_VARIABLES 32      /* Max variables per message */
#define IP_HEADER_MAX_VAR_NAME  50      /* Max variable name length */
#define IP_HEADER_MAX_VAR_VALUE 256     /* Max variable value length */
#define IP_HEADER_MAX_UNIT      25      /* Max unit string length */
#define IP_HEADER_MAX_PAYLOAD   512     /* Max total payload size */
#define IP_HEADER_FRAME_DELIM   '|'
#define IP_HEADER_VAR_DELIM     ';'
#define IP_HEADER_BLOCK_OPEN    '['
#define IP_HEADER_BLOCK_CLOSE   ']'

/* ==================== Data Type Definitions ==================== */

/**
 * Data type operators - similar to TagoTiP
 * This allows flexible representation of different sensor values
 */
typedef enum {
    IP_TYPE_NUMBER   = ':',  /* :=  Numeric value (int/float) */
    IP_TYPE_STRING   = '=',  /* =   String value */
    IP_TYPE_BOOLEAN  = '?',  /* ?=  Boolean (true/false) */
    IP_TYPE_LOCATION = '@'   /* @=  Location (lat,lng,alt) */
} ip_data_type_t;

/**
 * Sensor data types - specific to grain silo and other IoT applications
 */
typedef enum {
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_PRESSURE,
    SENSOR_MOISTURE,
    SENSOR_LIGHT,
    SENSOR_CO2,
    SENSOR_GAS,
    SENSOR_LEVEL,      /* Grain level in silo */
    SENSOR_WEIGHT,
    SENSOR_COUNT,      /* Generic counter */
    SENSOR_STATUS,     /* Generic status */
    SENSOR_CUSTOM      /* User-defined */
} sensor_type_t;

/* ==================== Structures ==================== */

/**
 * Variable metadata
 * Represents a single data point with all optional suffixes
 */
typedef struct {
    char name[IP_HEADER_MAX_VAR_NAME];      /* Variable name */
    ip_data_type_t type;                     /* Data type operator */
    char value[IP_HEADER_MAX_VAR_VALUE];    /* Variable value */
    char unit[IP_HEADER_MAX_UNIT];          /* Optional: unit suffix (#) */
    uint64_t timestamp;                      /* Optional: timestamp in ms (@) */
    char metadata[IP_HEADER_MAX_VAR_VALUE]; /* Optional: metadata ({}) */
    sensor_type_t sensor_type;               /* Sensor classification */
    bool has_unit;
    bool has_timestamp;
    bool has_metadata;
} ip_variable_t;

/**
 * IP Header frame
 * Represents the complete message frame
 */
typedef struct {
    uint8_t version;                         /* Protocol version */
    char device_id[IP_HEADER_MAX_VAR_NAME]; /* Device identifier */
    ip_variable_t variables[IP_HEADER_MAX_VARIABLES];
    uint16_t variable_count;
    uint32_t sequence_num;                   /* Optional sequence counter */
    bool has_sequence;
    uint64_t created_at;                     /* Frame creation timestamp */
} ip_frame_t;

/* ==================== Function Declarations ==================== */

/**
 * Initialize an IP header frame
 * @param frame Pointer to frame structure
 * @param device_id Device identifier string
 * @return 0 on success, -1 on error
 */
int ip_frame_init(ip_frame_t *frame, const char *device_id);

/**
 * Add a variable to the frame
 * @param frame Pointer to frame structure
 * @param name Variable name
 * @param type Data type operator
 * @param value Variable value
 * @param sensor_type Sensor type classification
 * @return 0 on success, -1 if frame is full or invalid input
 */
int ip_frame_add_variable(ip_frame_t *frame, const char *name, 
                          ip_data_type_t type, const char *value,
                          sensor_type_t sensor_type);

/**
 * Add unit suffix to last added variable
 * @param frame Pointer to frame structure
 * @param unit Unit string (e.g., "C", "%", "hPa")
 * @return 0 on success, -1 if no variables or invalid input
 */
int ip_variable_set_unit(ip_frame_t *frame, const char *unit);

/**
 * Set timestamp for last added variable
 * @param frame Pointer to frame structure
 * @param timestamp_ms Timestamp in milliseconds
 * @return 0 on success, -1 if no variables
 */
int ip_variable_set_timestamp(ip_frame_t *frame, uint64_t timestamp_ms);

/**
 * Set metadata for last added variable
 * @param frame Pointer to frame structure
 * @param metadata Metadata string (key=value format)
 * @return 0 on success, -1 if no variables or metadata too long
 */
int ip_variable_set_metadata(ip_frame_t *frame, const char *metadata);

/**
 * Set sequence counter for the frame
 * @param frame Pointer to frame structure
 * @param seq_num Sequence number
 */
void ip_frame_set_sequence(ip_frame_t *frame, uint32_t seq_num);

/**
 * Encode frame to text representation (similar to TagoTiP)
 * Example: "PUSH|device-01|[temp:=32.5#C;humidity:=65#%]"
 * 
 * @param frame Pointer to frame structure
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param add_sequence Whether to include sequence number
 * @return Length of encoded data on success, -1 on error
 */
int ip_frame_encode_text(const ip_frame_t *frame, char *buffer, 
                         size_t buffer_size, bool add_sequence);

/**
 * Encode frame to binary representation
 * Binary format:
 * [version:1][device_id_len:1][device_id:N][var_count:1]
 * [for each variable: type:1|name_len:1|name:N|value_len:2|value:N|...]
 * 
 * @param frame Pointer to frame structure
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Length of encoded data on success, -1 on error
 */
int ip_frame_encode_binary(const ip_frame_t *frame, uint8_t *buffer,
                           size_t buffer_size);

/**
 * Decode text frame (parse TagoTiP-like format)
 * 
 * @param buffer Input buffer with text frame
 * @param buffer_size Size of input buffer
 * @param frame Pointer to frame structure to populate
 * @return 0 on success, -1 on parse error
 */
int ip_frame_decode_text(const char *buffer, size_t buffer_size,
                         ip_frame_t *frame);

/**
 * Decode binary frame
 * 
 * @param buffer Input buffer with binary frame
 * @param buffer_size Size of input buffer
 * @param frame Pointer to frame structure to populate
 * @return 0 on success, -1 on parse error
 */
int ip_frame_decode_binary(const uint8_t *buffer, size_t buffer_size,
                           ip_frame_t *frame);

/**
 * Clear all variables from frame
 * @param frame Pointer to frame structure
 */
void ip_frame_clear_variables(ip_frame_t *frame);

/**
 * Get variable count in frame
 * @param frame Pointer to frame structure
 * @return Number of variables
 */
uint16_t ip_frame_get_variable_count(const ip_frame_t *frame);

/**
 * Get variable by index
 * @param frame Pointer to frame structure
 * @param index Variable index
 * @return Pointer to variable or NULL if index out of bounds
 */
const ip_variable_t *ip_frame_get_variable(const ip_frame_t *frame, 
                                           uint16_t index);

/**
 * Get sensor type name as string
 * @param sensor_type Sensor type
 * @return String representation of sensor type
 */
const char *ip_sensor_type_to_string(sensor_type_t sensor_type);

/**
 * Get data type operator as string
 * @param type Data type
 * @return String representation (":=", "=", "?=", "@=")
 */
const char *ip_data_type_to_operator(ip_data_type_t type);

#endif /* IP_HEADER_H */
