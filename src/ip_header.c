#include "ip_header.h"
#include <stdio.h>
#include <stdlib.h>

/* ==================== Internal Helper Functions ==================== */

/**
 * Validate variable name (alphanumeric and underscores)
 */
static bool ip_is_valid_var_name(const char *name)
{
    if (!name || strlen(name) == 0 || strlen(name) > IP_HEADER_MAX_VAR_NAME)
        return false;

    for (size_t i = 0; i < strlen(name); i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    }
    return true;
}

/**
 * Validate device ID
 */
static bool ip_is_valid_device_id(const char *device_id)
{
    if (!device_id || strlen(device_id) == 0 || strlen(device_id) > IP_HEADER_MAX_VAR_NAME)
        return false;
    return true;
}

/**
 * Escape special characters in values
 */
static void ip_escape_value(const char *src, char *dst, size_t dst_size)
{
    size_t src_idx = 0, dst_idx = 0;
    while (src[src_idx] != '\0' && dst_idx < dst_size - 1) {
        char c = src[src_idx];
        if (c == IP_HEADER_FRAME_DELIM || c == IP_HEADER_VAR_DELIM || 
            c == '#' || c == '@' || c == '^' || c == '{' || c == '}') {
            if (dst_idx < dst_size - 2) {
                dst[dst_idx++] = '\\';
                dst[dst_idx++] = c;
            }
        } else {
            dst[dst_idx++] = c;
        }
        src_idx++;
    }
    dst[dst_idx] = '\0';
}

/**
 * Unescape special characters in values
 */
static void ip_unescape_value(const char *src, char *dst, size_t dst_size)
{
    size_t src_idx = 0, dst_idx = 0;
    while (src[src_idx] != '\0' && dst_idx < dst_size - 1) {
        if (src[src_idx] == '\\' && src[src_idx + 1] != '\0') {
            dst[dst_idx++] = src[src_idx + 1];
            src_idx += 2;
        } else {
            dst[dst_idx++] = src[src_idx++];
        }
    }
    dst[dst_idx] = '\0';
}

/* ==================== Frame Initialization ==================== */

int ip_frame_init(ip_frame_t *frame, const char *device_id)
{
    if (!frame || !ip_is_valid_device_id(device_id))
        return -1;

    frame->version = IP_HEADER_VERSION;
    strncpy(frame->device_id, device_id, IP_HEADER_MAX_VAR_NAME - 1);
    frame->device_id[IP_HEADER_MAX_VAR_NAME - 1] = '\0';
    frame->variable_count = 0;
    frame->sequence_num = 0;
    frame->has_sequence = false;
    frame->created_at = 0; /* Set by application if needed */

    return 0;
}

/* ==================== Variable Management ==================== */

int ip_frame_add_variable(ip_frame_t *frame, const char *name,
                          ip_data_type_t type, const char *value,
                          sensor_type_t sensor_type)
{
    if (!frame || !ip_is_valid_var_name(name) || !value)
        return -1;

    if (frame->variable_count >= IP_HEADER_MAX_VARIABLES)
        return -1;

    if (strlen(value) > IP_HEADER_MAX_VAR_VALUE)
        return -1;

    ip_variable_t *var = &frame->variables[frame->variable_count];

    strncpy(var->name, name, IP_HEADER_MAX_VAR_NAME - 1);
    var->name[IP_HEADER_MAX_VAR_NAME - 1] = '\0';

    var->type = type;

    ip_escape_value(value, var->value, IP_HEADER_MAX_VAR_VALUE);
    
    var->sensor_type = sensor_type;
    var->has_unit = false;
    var->has_timestamp = false;
    var->has_metadata = false;
    var->unit[0] = '\0';
    var->metadata[0] = '\0';
    var->timestamp = 0;

    frame->variable_count++;

    return 0;
}

int ip_variable_set_unit(ip_frame_t *frame, const char *unit)
{
    if (!frame || frame->variable_count == 0 || !unit)
        return -1;

    if (strlen(unit) > IP_HEADER_MAX_UNIT - 1)
        return -1;

    ip_variable_t *var = &frame->variables[frame->variable_count - 1];
    strncpy(var->unit, unit, IP_HEADER_MAX_UNIT - 1);
    var->unit[IP_HEADER_MAX_UNIT - 1] = '\0';
    var->has_unit = true;

    return 0;
}

int ip_variable_set_timestamp(ip_frame_t *frame, uint64_t timestamp_ms)
{
    if (!frame || frame->variable_count == 0)
        return -1;

    ip_variable_t *var = &frame->variables[frame->variable_count - 1];
    var->timestamp = timestamp_ms;
    var->has_timestamp = true;

    return 0;
}

int ip_variable_set_metadata(ip_frame_t *frame, const char *metadata)
{
    if (!frame || frame->variable_count == 0 || !metadata)
        return -1;

    if (strlen(metadata) > IP_HEADER_MAX_VAR_VALUE - 1)
        return -1;

    ip_variable_t *var = &frame->variables[frame->variable_count - 1];
    strncpy(var->metadata, metadata, IP_HEADER_MAX_VAR_VALUE - 1);
    var->metadata[IP_HEADER_MAX_VAR_VALUE - 1] = '\0';
    var->has_metadata = true;

    return 0;
}

void ip_frame_set_sequence(ip_frame_t *frame, uint32_t seq_num)
{
    if (!frame)
        return;

    frame->sequence_num = seq_num;
    frame->has_sequence = true;
}

/* ==================== Frame Encoding ==================== */

int ip_frame_encode_text(const ip_frame_t *frame, char *buffer,
                         size_t buffer_size, bool add_sequence)
{
    if (!frame || !buffer || buffer_size < 10)
        return -1;

    int pos = 0;

    /* Method (always PUSH for now) */
    pos += snprintf(buffer + pos, buffer_size - pos, "PUSH");

    /* Sequence (optional) */
    if (add_sequence && frame->has_sequence) {
        pos += snprintf(buffer + pos, buffer_size - pos, "|!%u", frame->sequence_num);
    }

    /* Device ID */
    pos += snprintf(buffer + pos, buffer_size - pos, "|%s", frame->device_id);

    /* Variable block */
    pos += snprintf(buffer + pos, buffer_size - pos, "|[");

    for (uint16_t i = 0; i < frame->variable_count; i++) {
        const ip_variable_t *var = &frame->variables[i];

        if (pos >= (int)buffer_size - 100)
            return -1; /* Buffer overflow protection */

        if (i > 0)
            pos += snprintf(buffer + pos, buffer_size - pos, ";");

        /* Variable name and operator */
        const char *op = ip_data_type_to_operator(var->type);
        pos += snprintf(buffer + pos, buffer_size - pos, "%s%s%s",
                       var->name, op, var->value);

        /* Unit suffix */
        if (var->has_unit) {
            pos += snprintf(buffer + pos, buffer_size - pos, "#%s", var->unit);
        }

        /* Timestamp suffix */
        if (var->has_timestamp) {
            pos += snprintf(buffer + pos, buffer_size - pos, "@%llu",
                           (unsigned long long)var->timestamp);
        }

        /* Metadata suffix */
        if (var->has_metadata) {
            pos += snprintf(buffer + pos, buffer_size - pos, "{%s}", var->metadata);
        }
    }

    pos += snprintf(buffer + pos, buffer_size - pos, "]");

    if (pos >= (int)buffer_size)
        return -1;

    return pos;
}

int ip_frame_encode_binary(const ip_frame_t *frame, uint8_t *buffer,
                           size_t buffer_size)
{
    if (!frame || !buffer || buffer_size < 10)
        return -1;

    size_t pos = 0;

    /* Version */
    if (pos >= buffer_size)
        return -1;
    buffer[pos++] = frame->version;

    /* Device ID length and data */
    uint8_t device_id_len = strlen(frame->device_id);
    if (pos + 1 + device_id_len >= buffer_size)
        return -1;
    buffer[pos++] = device_id_len;
    memcpy(buffer + pos, frame->device_id, device_id_len);
    pos += device_id_len;

    /* Sequence number (optional) */
    if (frame->has_sequence) {
        if (pos + 4 >= buffer_size)
            return -1;
        buffer[pos++] = 1; /* Has sequence marker */
        buffer[pos++] = (frame->sequence_num >> 24) & 0xFF;
        buffer[pos++] = (frame->sequence_num >> 16) & 0xFF;
        buffer[pos++] = (frame->sequence_num >> 8) & 0xFF;
        buffer[pos++] = frame->sequence_num & 0xFF;
    } else {
        if (pos >= buffer_size)
            return -1;
        buffer[pos++] = 0; /* No sequence marker */
    }

    /* Variable count */
    if (pos >= buffer_size)
        return -1;
    buffer[pos++] = frame->variable_count;

    /* Variables */
    for (uint16_t i = 0; i < frame->variable_count; i++) {
        const ip_variable_t *var = &frame->variables[i];

        if (pos + 20 >= buffer_size) /* Minimum space needed */
            return -1;

        /* Variable type */
        buffer[pos++] = (uint8_t)var->type;

        /* Sensor type */
        buffer[pos++] = (uint8_t)var->sensor_type;

        /* Variable name length and data */
        uint8_t name_len = strlen(var->name);
        buffer[pos++] = name_len;
        if (pos + name_len >= buffer_size)
            return -1;
        memcpy(buffer + pos, var->name, name_len);
        pos += name_len;

        /* Variable value length (16-bit) and data */
        uint16_t value_len = strlen(var->value);
        if (pos + 2 + value_len >= buffer_size)
            return -1;
        buffer[pos++] = (value_len >> 8) & 0xFF;
        buffer[pos++] = value_len & 0xFF;
        memcpy(buffer + pos, var->value, value_len);
        pos += value_len;

        /* Unit (optional) */
        if (var->has_unit) {
            uint8_t unit_len = strlen(var->unit);
            if (pos + 1 + unit_len >= buffer_size)
                return -1;
            buffer[pos++] = unit_len;
            memcpy(buffer + pos, var->unit, unit_len);
            pos += unit_len;
        } else {
            buffer[pos++] = 0;
        }

        /* Timestamp (optional, 8 bytes) */
        if (var->has_timestamp) {
            if (pos + 8 >= buffer_size)
                return -1;
            buffer[pos++] = 1; /* Has timestamp marker */
            buffer[pos++] = (var->timestamp >> 56) & 0xFF;
            buffer[pos++] = (var->timestamp >> 48) & 0xFF;
            buffer[pos++] = (var->timestamp >> 40) & 0xFF;
            buffer[pos++] = (var->timestamp >> 32) & 0xFF;
            buffer[pos++] = (var->timestamp >> 24) & 0xFF;
            buffer[pos++] = (var->timestamp >> 16) & 0xFF;
            buffer[pos++] = (var->timestamp >> 8) & 0xFF;
            buffer[pos++] = var->timestamp & 0xFF;
        } else {
            if (pos >= buffer_size)
                return -1;
            buffer[pos++] = 0;
        }

        /* Metadata (optional) */
        if (var->has_metadata) {
            uint16_t meta_len = strlen(var->metadata);
            if (pos + 2 + meta_len >= buffer_size)
                return -1;
            buffer[pos++] = (meta_len >> 8) & 0xFF;
            buffer[pos++] = meta_len & 0xFF;
            memcpy(buffer + pos, var->metadata, meta_len);
            pos += meta_len;
        } else {
            if (pos + 2 >= buffer_size)
                return -1;
            buffer[pos++] = 0;
            buffer[pos++] = 0;
        }
    }

    return pos;
}

/* ==================== Frame Decoding ==================== */

int ip_frame_decode_text(const char *buffer, size_t buffer_size,
                         ip_frame_t *frame)
{
    if (!buffer || buffer_size == 0 || !frame)
        return -1;

    /* This is a simplified parser. Full implementation would handle
     * all escape sequences and edge cases like TagoTiP */

    char temp_buffer[IP_HEADER_MAX_PAYLOAD];
    strncpy(temp_buffer, buffer, sizeof(temp_buffer) - 1);
    temp_buffer[sizeof(temp_buffer) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(temp_buffer, "|", &saveptr);

    if (!token || strcmp(token, "PUSH") != 0)
        return -1;

    /* Check for sequence */
    token = strtok_r(NULL, "|", &saveptr);
    if (token[0] == '!') {
        frame->sequence_num = atoi(token + 1);
        frame->has_sequence = true;
        token = strtok_r(NULL, "|", &saveptr);
    }

    /* Device ID */
    if (!token || !ip_is_valid_device_id(token))
        return -1;

    strncpy(frame->device_id, token, IP_HEADER_MAX_VAR_NAME - 1);
    frame->device_id[IP_HEADER_MAX_VAR_NAME - 1] = '\0';

    /* Variable block */
    token = strtok_r(NULL, "|", &saveptr);
    if (!token || token[0] != '[')
        return -1;

    /* Remove brackets */
    if (token[strlen(token) - 1] != ']')
        return -1;

    token[strlen(token) - 1] = '\0';
    token++; /* Skip '[' */

    /* Parse variables (simplified - full implementation needed) */
    char *var_saveptr = NULL;
    char *var_token = strtok_r(token, ";", &var_saveptr);

    while (var_token && frame->variable_count < IP_HEADER_MAX_VARIABLES) {
        /* Parse individual variable (name:=value#unit@timestamp)
         * This is simplified - full implementation would handle all cases */
        
        char *op_pos = strchr(var_token, ':');
        if (!op_pos)
            op_pos = strchr(var_token, '=');
        if (!op_pos)
            op_pos = strchr(var_token, '?');
        if (!op_pos)
            op_pos = strchr(var_token, '@');

        if (!op_pos)
            return -1;

        char var_name[IP_HEADER_MAX_VAR_NAME];
        strncpy(var_name, var_token, op_pos - var_token);
        var_name[op_pos - var_token] = '\0';

        ip_data_type_t type = (ip_data_type_t)*op_pos;

        /* Simple value extraction (would need more robust parsing) */
        char *value_start = op_pos + 2; /* Skip operator */
        char var_value[IP_HEADER_MAX_VAR_VALUE];
        strncpy(var_value, value_start, sizeof(var_value) - 1);
        var_value[sizeof(var_value) - 1] = '\0';

        if (ip_frame_add_variable(frame, var_name, type, var_value, SENSOR_CUSTOM) != 0)
            return -1;

        var_token = strtok_r(NULL, ";", &var_saveptr);
    }

    return 0;
}

int ip_frame_decode_binary(const uint8_t *buffer, size_t buffer_size,
                           ip_frame_t *frame)
{
    if (!buffer || buffer_size < 5 || !frame)
        return -1;

    size_t pos = 0;

    /* Version */
    frame->version = buffer[pos++];

    /* Device ID */
    uint8_t device_id_len = buffer[pos++];
    if (pos + device_id_len > buffer_size)
        return -1;
    memcpy(frame->device_id, buffer + pos, device_id_len);
    frame->device_id[device_id_len] = '\0';
    pos += device_id_len;

    /* Sequence number */
    if (pos >= buffer_size)
        return -1;
    if (buffer[pos++] == 1) {
        if (pos + 4 > buffer_size)
            return -1;
        frame->sequence_num = ((uint32_t)buffer[pos] << 24) |
                             ((uint32_t)buffer[pos + 1] << 16) |
                             ((uint32_t)buffer[pos + 2] << 8) |
                             (uint32_t)buffer[pos + 3];
        frame->has_sequence = true;
        pos += 4;
    } else {
        frame->has_sequence = false;
    }

    /* Variable count */
    if (pos >= buffer_size)
        return -1;
    frame->variable_count = buffer[pos++];

    /* Variables */
    for (uint16_t i = 0; i < frame->variable_count && i < IP_HEADER_MAX_VARIABLES; i++) {
        if (pos + 5 > buffer_size)
            return -1;

        ip_variable_t *var = &frame->variables[i];

        /* Type and sensor type */
        var->type = (ip_data_type_t)buffer[pos++];
        var->sensor_type = (sensor_type_t)buffer[pos++];

        /* Variable name */
        uint8_t name_len = buffer[pos++];
        if (pos + name_len > buffer_size)
            return -1;
        memcpy(var->name, buffer + pos, name_len);
        var->name[name_len] = '\0';
        pos += name_len;

        /* Variable value */
        if (pos + 2 > buffer_size)
            return -1;
        uint16_t value_len = ((uint16_t)buffer[pos] << 8) | buffer[pos + 1];
        pos += 2;
        if (pos + value_len > buffer_size)
            return -1;
        memcpy(var->value, buffer + pos, value_len);
        var->value[value_len] = '\0';
        pos += value_len;

        /* Unit */
        if (pos >= buffer_size)
            return -1;
        uint8_t unit_len = buffer[pos++];
        if (unit_len > 0) {
            if (pos + unit_len > buffer_size)
                return -1;
            memcpy(var->unit, buffer + pos, unit_len);
            var->unit[unit_len] = '\0';
            var->has_unit = true;
            pos += unit_len;
        } else {
            var->has_unit = false;
            var->unit[0] = '\0';
        }

        /* Timestamp */
        if (pos >= buffer_size)
            return -1;
        if (buffer[pos++] == 1) {
            if (pos + 8 > buffer_size)
                return -1;
            var->timestamp = ((uint64_t)buffer[pos] << 56) |
                            ((uint64_t)buffer[pos + 1] << 48) |
                            ((uint64_t)buffer[pos + 2] << 40) |
                            ((uint64_t)buffer[pos + 3] << 32) |
                            ((uint64_t)buffer[pos + 4] << 24) |
                            ((uint64_t)buffer[pos + 5] << 16) |
                            ((uint64_t)buffer[pos + 6] << 8) |
                            (uint64_t)buffer[pos + 7];
            var->has_timestamp = true;
            pos += 8;
        } else {
            var->has_timestamp = false;
        }

        /* Metadata */
        if (pos + 2 > buffer_size)
            return -1;
        uint16_t meta_len = ((uint16_t)buffer[pos] << 8) | buffer[pos + 1];
        pos += 2;
        if (meta_len > 0) {
            if (pos + meta_len > buffer_size)
                return -1;
            memcpy(var->metadata, buffer + pos, meta_len);
            var->metadata[meta_len] = '\0';
            var->has_metadata = true;
            pos += meta_len;
        } else {
            var->has_metadata = false;
            var->metadata[0] = '\0';
        }
    }

    return 0;
}

/* ==================== Utility Functions ==================== */

void ip_frame_clear_variables(ip_frame_t *frame)
{
    if (!frame)
        return;
    frame->variable_count = 0;
}

uint16_t ip_frame_get_variable_count(const ip_frame_t *frame)
{
    if (!frame)
        return 0;
    return frame->variable_count;
}

const ip_variable_t *ip_frame_get_variable(const ip_frame_t *frame,
                                           uint16_t index)
{
    if (!frame || index >= frame->variable_count)
        return NULL;
    return &frame->variables[index];
}

const char *ip_sensor_type_to_string(sensor_type_t sensor_type)
{
    switch (sensor_type) {
        case SENSOR_TEMPERATURE: return "temperature";
        case SENSOR_HUMIDITY:    return "humidity";
        case SENSOR_PRESSURE:    return "pressure";
        case SENSOR_MOISTURE:    return "moisture";
        case SENSOR_LIGHT:       return "light";
        case SENSOR_CO2:         return "co2";
        case SENSOR_GAS:         return "gas";
        case SENSOR_LEVEL:       return "level";
        case SENSOR_WEIGHT:      return "weight";
        case SENSOR_COUNT:       return "count";
        case SENSOR_STATUS:      return "status";
        case SENSOR_CUSTOM:      return "custom";
        default:                 return "unknown";
    }
}

const char *ip_data_type_to_operator(ip_data_type_t type)
{
    switch (type) {
        case IP_TYPE_NUMBER:   return ":=";
        case IP_TYPE_STRING:   return "=";
        case IP_TYPE_BOOLEAN:  return "?=";
        case IP_TYPE_LOCATION: return "@=";
        default:               return "?";
    }
}
