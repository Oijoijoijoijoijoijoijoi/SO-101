#ifndef SO_101_TYPES_H
#define SO_101_TYPES_H

#include <stdint.h>

typedef enum {
    SOURCE_LEADER = 0,
    SOURCE_FOLLOWER = 1
} device_origin_t;

/**
 * @brief Raw Telemetry payload. 
 * Total size: 21 bytes.
 */
typedef struct __attribute__((packed)) {
    uint8_t origin_id;         // 1 byte
    uint32_t sequence_id;      // 4 bytes
    uint16_t raw_positions[6]; // 12 bytes
    uint32_t timestamp_ms;     // 4 bytes
} telemetry_data_t;

/**
 * @brief Bundled payload for 802.11 Action Frames.
 * Total size: 42 bytes.
 */
typedef struct __attribute__((packed)) {
    telemetry_data_t leader;
    telemetry_data_t follower;
} dual_telemetry_t;

#endif // SO_101_TYPES_H