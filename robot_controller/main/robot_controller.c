#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "so_101_types.h"

static const char *TAG = "ROBOT_CONTROLLER";
static uint32_t failure_count = 0;

/* --- Link Layer Configuration --- */
static uint8_t gateway_mac[] = {0x44, 0x1D, 0x64, 0xF6, 0xFF, 0xFC};

/* --- Kinematic Simulation Constants --- */
#define PHASE_INCREMENT 0.0628f  // Δθ per step (~0.5Hz oscillation at 50Hz)
#define AMPLITUDE 500.0f         // Magnitude in raw ADC units
#define CENTER_VALUE 2048        // 12-bit midpoint

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        failure_count++;
        if (failure_count % 200 == 0) {
            ESP_LOGE(TAG, "TX Failure (Total: %" PRIu32 ") for Peer: " MACSTR, 
                     failure_count, MAC2STR(mac_addr));
        }
    }
}

void init_wifi_espnow(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer_info = {
        .channel = 1,
        .encrypt = false,
        .ifidx = WIFI_IF_STA
    };
    memcpy(peer_info.peer_addr, gateway_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

void app_main(void) {
    init_wifi_espnow();
    
    telemetry_data_t leader_data = { .origin_id = SOURCE_LEADER, .sequence_id = 0 };
    telemetry_data_t follower_data = { .origin_id = SOURCE_FOLLOWER, .sequence_id = 0 };
    
    float theta = 0.0f;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz frequency

    while(1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

        theta += PHASE_INCREMENT;
        if (theta > 2.0f * (float)M_PI) theta -= 2.0f * (float)M_PI;

        // --- Leader Data Dispatch ---
        leader_data.sequence_id++;
        leader_data.timestamp_ms = now;
        for(int i = 0; i < 6; i++) {
            leader_data.raw_positions[i] = (uint16_t)(CENTER_VALUE + (AMPLITUDE * sinf(theta + (i * 0.5f))));
        }
        esp_now_send(gateway_mac, (uint8_t *)&leader_data, sizeof(leader_data));

        // --- Follower Data Dispatch ---
        follower_data.sequence_id++;
        follower_data.timestamp_ms = now;
        for(int i = 0; i < 6; i++) {
            follower_data.raw_positions[i] = (uint16_t)(CENTER_VALUE + (AMPLITUDE * sinf(theta + (i * 0.5f) - 0.2f)));
        }
        esp_now_send(gateway_mac, (uint8_t *)&follower_data, sizeof(follower_data));

        if (leader_data.sequence_id % 200 == 0) {
            ESP_LOGI(TAG, "Heartbeat: Sequence %" PRIu32 " dispatched on Channel 1", leader_data.sequence_id);
        }
    }
}