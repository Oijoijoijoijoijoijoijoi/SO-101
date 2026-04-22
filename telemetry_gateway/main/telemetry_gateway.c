#include <string.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_log.h"
#include "so_101_types.h"

static const char *TAG = "GATEWAY_BRIDGE";

#define RAW_TO_RAD (2.0f * 3.14159265f / 4096.0f)
#define RAW_CENTER 2048

/**
 * @brief Transform raw ADC values to SI radians and log results.
 * This is now rate-limited by the caller to prevent UART saturation.
 */
void process_telemetry_to_ros(const telemetry_data_t *data) {
    float joint_state_radians[6];
    for (int i = 0; i < 6; i++) {
        joint_state_radians[i] = (float)((int32_t)data->raw_positions[i] - RAW_CENTER) * RAW_TO_RAD;
        ESP_LOGI(TAG, "Joint [%d] Raw: %u -> Rad: %.4f", i, data->raw_positions[i], joint_state_radians[i]);
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(telemetry_data_t)) {
        telemetry_data_t *received_data = (telemetry_data_t *)data;
        
        // Decimate processing to 1Hz (every 50th packet) for serial monitoring
        if (received_data->sequence_id % 200 == 0) {
            ESP_LOGI(TAG, "--------------------------------------------------");
            ESP_LOGI(TAG, "Health Check | Packet ID: %" PRIu32 " | Source: " MACSTR, 
                     received_data->sequence_id, 
                     MAC2STR(recv_info->src_addr));
            
            process_telemetry_to_ros(received_data);
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

    // Force synchronization to Channel 1 (Standard ESP-NOW requirement)
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "Gateway MAC Address: " MACSTR, MAC2STR(mac));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}

void app_main(void) {
    init_wifi_espnow();
}