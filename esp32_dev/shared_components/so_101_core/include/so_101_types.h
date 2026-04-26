#ifndef SO_101_TYPES_H
#define SO_101_TYPES_H

#include <stdint.h>

typedef enum {
    SOURCE_LEADER = 0,
    SOURCE_FOLLOWER = 1
} device_origin_t;

/**
 * @brief Raw Telemetry payload imitating Feetech status packets.
 * 250-byte ESP-NOW limit is well-respected (current size: 20 bytes).
 */
typedef struct __attribute__((packed)) {
    uint8_t origin_id;         // Identification tag (1 byte)
    uint32_t sequence_id;      // Sequence (4 bytes)
    uint16_t raw_positions[6]; // Position array (12 bytes)
    uint32_t timestamp_ms;     // Timestamp (4 bytes)
} telemetry_data_t;

#endif // SO_101_TYPES_H