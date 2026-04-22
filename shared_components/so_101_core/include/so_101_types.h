#ifndef SO_101_TYPES_H
#define SO_101_TYPES_H

#include <stdint.h>

/**
 * @brief Raw Telemetry payload imitating Feetech status packets.
 * 250-byte ESP-NOW limit is well-respected (current size: 20 bytes).
 */
typedef struct __attribute__((packed)) {
    uint32_t sequence_id;      // 4 bytes
    uint16_t raw_positions[6]; // 12 bytes (2 per joint, 0-4095)
    uint32_t timestamp_ms;     // 4 bytes
} telemetry_data_t;

#endif // SO_101_TYPES_H